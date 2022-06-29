/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//内臓されている userChrome.js をロード
var firefox = document.createElement('script');
firefox.src = "chrome://userchromejs/content/chromecss.uc.js"; 
document.head.appendChild(firefox); 

var firefox = document.createElement('script');
firefox.src = "chrome://userchromejs/content/sidebarautohide.uc.js"; 
document.head.appendChild(firefox); 
