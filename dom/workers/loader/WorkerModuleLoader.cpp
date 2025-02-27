/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/experimental/JSStencil.h"  // JS::Stencil, JS::CompileModuleScriptToStencil, JS::InstantiateModuleStencil
#include "js/loader/ModuleLoadRequest.h"
#include "mozilla/dom/WorkerLoadContext.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/workerinternals/ScriptLoader.h"
#include "mozilla/dom/WorkerScope.h"
#include "WorkerModuleLoader.h"

#include "nsISupportsImpl.h"

namespace mozilla::dom::workerinternals::loader {

//////////////////////////////////////////////////////////////
// WorkerModuleLoader
//////////////////////////////////////////////////////////////

NS_IMPL_ADDREF_INHERITED(WorkerModuleLoader, JS::loader::ModuleLoaderBase)
NS_IMPL_RELEASE_INHERITED(WorkerModuleLoader, JS::loader::ModuleLoaderBase)

NS_IMPL_CYCLE_COLLECTION_INHERITED(WorkerModuleLoader,
                                   JS::loader::ModuleLoaderBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkerModuleLoader)
NS_INTERFACE_MAP_END_INHERITING(JS::loader::ModuleLoaderBase)

WorkerModuleLoader::WorkerModuleLoader(WorkerScriptLoader* aScriptLoader,
                                       nsIGlobalObject* aGlobalObject,
                                       nsISerialEventTarget* aEventTarget)
    : ModuleLoaderBase(aScriptLoader, aGlobalObject, aEventTarget) {}

nsIURI* WorkerModuleLoader::GetBaseURI() const {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  return workerPrivate->GetBaseURI();
}

already_AddRefed<ModuleLoadRequest> WorkerModuleLoader::CreateStaticImport(
    nsIURI* aURI, ModuleLoadRequest* aParent) {
  // We are intentionally deviating from the specification here and using the
  // worker's CSP rather than the document CSP. The spec otherwise requires our
  // service worker integration to be changed, and additionally the decision
  // here did not make sense as we are treating static imports as different from
  // other kinds of subresources.
  // See Discussion in https://github.com/w3c/webappsec-csp/issues/336
  Maybe<ClientInfo> clientInfo = GetGlobalObject()->GetClientInfo();

  RefPtr<WorkerLoadContext> loadContext = new WorkerLoadContext(
      WorkerLoadContext::Kind::StaticImport, clientInfo,
      aParent->GetWorkerLoadContext()->mScriptLoader,
      aParent->GetWorkerLoadContext()->mOnlyExistingCachedResourcesAllowed);
  RefPtr<ModuleLoadRequest> request = new ModuleLoadRequest(
      aURI, aParent->mFetchOptions, SRIMetadata(), aParent->mURI, loadContext,
      false, /* is top level */
      false, /* is dynamic import */
      this, aParent->mVisitedSet, aParent->GetRootModule());

  request->mURL = request->mURI->GetSpecOrDefault();
  return request.forget();
}

bool WorkerModuleLoader::CreateDynamicImportLoader() {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  workerPrivate->AssertIsOnWorkerThread();

  IgnoredErrorResult rv;
  RefPtr<WorkerScriptLoader> loader = new loader::WorkerScriptLoader(
      workerPrivate, nullptr, nullptr,
      GetCurrentScriptLoader()->GetWorkerScriptType(), rv);
  if (NS_WARN_IF(rv.Failed())) {
    return false;
  }

  SetScriptLoader(loader);
  SetEventTarget(GetCurrentSerialEventTarget());
  return true;
}

already_AddRefed<ModuleLoadRequest> WorkerModuleLoader::CreateDynamicImport(
    JSContext* aCx, nsIURI* aURI, LoadedScript* aMaybeActiveScript,
    JS::Handle<JS::Value> aReferencingPrivate, JS::Handle<JSString*> aSpecifier,
    JS::Handle<JSObject*> aPromise) {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();

  if (!CreateDynamicImportLoader()) {
    return nullptr;
  }

  // Not supported for Service Workers.
  // https://github.com/w3c/ServiceWorker/issues/1585 covers existing discussion
  // about potentially supporting use of import().
  if (workerPrivate->IsServiceWorker()) {
    return nullptr;
  }
  MOZ_ASSERT(aSpecifier);
  MOZ_ASSERT(aPromise);

  RefPtr<ScriptFetchOptions> options;
  nsIURI* baseURL = nullptr;
  if (aMaybeActiveScript) {
    options = aMaybeActiveScript->GetFetchOptions();
    baseURL = aMaybeActiveScript->BaseURL();
  } else {
    ReferrerPolicy referrerPolicy = workerPrivate->GetReferrerPolicy();
    options =
        new ScriptFetchOptions(CORSMode::CORS_NONE, referrerPolicy, nullptr);
    baseURL = GetBaseURI();
  }

  Maybe<ClientInfo> clientInfo = GetGlobalObject()->GetClientInfo();

  RefPtr<WorkerLoadContext> context = new WorkerLoadContext(
      WorkerLoadContext::Kind::DynamicImport, clientInfo,
      GetCurrentScriptLoader(),
      // When dynamic import is supported in ServiceWorkers,
      // the current plan in onlyExistingCachedResourcesAllowed
      // is that only existing cached resources will be
      // allowed.  (`import()` will not be used for caching
      // side effects, but instead a specific method will be
      // used during installation.)
      true);

  RefPtr<ModuleLoadRequest> request = new ModuleLoadRequest(
      aURI, options, SRIMetadata(), baseURL, context, true,
      /* is top level */ true, /* is dynamic import */
      this, ModuleLoadRequest::NewVisitedSetForTopLevelImport(aURI), nullptr);

  request->mDynamicReferencingPrivate = aReferencingPrivate;
  request->mDynamicSpecifier = aSpecifier;
  request->mDynamicPromise = aPromise;

  HoldJSObjects(request.get());

  return request.forget();
}

bool WorkerModuleLoader::CanStartLoad(ModuleLoadRequest* aRequest,
                                      nsresult* aRvOut) {
  return true;
}

nsresult WorkerModuleLoader::StartFetch(ModuleLoadRequest* aRequest) {
  if (!GetScriptLoaderFor(aRequest)->DispatchLoadScript(aRequest)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult WorkerModuleLoader::CompileFetchedModule(
    JSContext* aCx, JS::Handle<JSObject*> aGlobal, JS::CompileOptions& aOptions,
    ModuleLoadRequest* aRequest, JS::MutableHandle<JSObject*> aModuleScript) {
  RefPtr<JS::Stencil> stencil;
  MOZ_ASSERT(aRequest->IsTextSource());
  MaybeSourceText maybeSource;
  nsresult rv = aRequest->GetScriptSource(aCx, &maybeSource);
  NS_ENSURE_SUCCESS(rv, rv);

  auto compile = [&](auto& source) {
    return JS::CompileModuleScriptToStencil(aCx, aOptions, source);
  };
  stencil = maybeSource.mapNonEmpty(compile);

  if (!stencil) {
    return NS_ERROR_FAILURE;
  }

  JS::InstantiateOptions instantiateOptions(aOptions);
  aModuleScript.set(
      JS::InstantiateModuleStencil(aCx, instantiateOptions, stencil));
  if (!aModuleScript) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

WorkerScriptLoader* WorkerModuleLoader::GetCurrentScriptLoader() {
  return static_cast<WorkerScriptLoader*>(mLoader.get());
}

WorkerScriptLoader* WorkerModuleLoader::GetScriptLoaderFor(
    ModuleLoadRequest* aRequest) {
  return aRequest->GetWorkerLoadContext()->mScriptLoader;
}

void WorkerModuleLoader::OnModuleLoadComplete(ModuleLoadRequest* aRequest) {
  if (aRequest->IsTopLevel()) {
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(GetGlobalObject()))) {
      return;
    }
    RefPtr<WorkerScriptLoader> requestScriptLoader =
        GetScriptLoaderFor(aRequest);
    if (aRequest->IsDynamicImport()) {
      aRequest->ProcessDynamicImport();
      requestScriptLoader->TryShutdown();
    } else {
      requestScriptLoader->MaybeMoveToLoadedList(aRequest);
      requestScriptLoader->ProcessPendingRequests(jsapi.cx());
    }
  }
}

bool WorkerModuleLoader::IsModuleEvaluationAborted(
    ModuleLoadRequest* aRequest) {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  return !workerPrivate || !workerPrivate->GlobalScope() ||
         workerPrivate->GlobalScope()->IsDying();
}

}  // namespace mozilla::dom::workerinternals::loader
