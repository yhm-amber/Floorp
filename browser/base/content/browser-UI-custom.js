/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//import utils
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// 並行して使えるデザイン設定の処理
//mica for everyone
if (Services.prefs.getBoolPref("floorp.material.effect.enable",false)){
    var Tag = document.createElement('style');
    Tag.innerText = `@import url(chrome://browser/skin/optioncss/micaforeveryone.css)`
    document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);   
  }
  
  // ツリー型タブ
  if(Services.prefs.getBoolPref("floorp.Tree-type.verticaltab.optimization",false)){
    var Tag = document.createElement('style');
    Tag.innerText = `@import url(chrome://browser/skin/optioncss/treestyletab.css)`
    document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
  }
  
  // side button 
  if (Services.prefs.getBoolPref("floorp.optimized.msbutton.ope", false)){
    var Tag = document.createElement('style');
    Tag.innerText = `@import url(chrome://browser/skin/optioncss/msbutton.css)`
    document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);  
  }
  
  //bookmarkbar auto hide
  if (Services.prefs.getBoolPref("floorp.bookmarks.bar.focus.mode", false)) {
    var Tag = document.createElement('style');
    Tag.innerText = `@import url(chrome://browser/skin/optioncss/bookmarkbar_autohide.css)`  
    document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
  }
  
  //Bootmark の位置を一番下に(fake statusbar)
  if (Services.prefs.getBoolPref("floorp.bookmarks.fakestatus.mode", false)) {
    var Tag = document.createElement('style');
    Tag.innerText = `@import url(chrome://browser/skin/optioncss/move_bookmarkbar.css)`
    document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
  }

  //ページ内部検索
  if (Services.prefs.getBoolPref("floorp.search.top.mode", false)) {
     var Tag = document.createElement('style');
     Tag.innerText = `@import url(chrome://browser/skin/optioncss/move_page_inside_searchbar.css)` 
     document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
  }
  
  // Classic menu
  if (Services.prefs.getBoolPref("floorp.legacy.menu.mode", false)) {
     var Tag = document.createElement('style');
     Tag.innerText = `@import url(chrome://browser/skin/optioncss/legacy_firefox_menu_mode.css)`
     document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
  }