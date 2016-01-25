
//

MenuItems = new Array();

MenuItems[0] = new Object();
MenuItems[0].Text = "Home";
MenuItems[0].Link = "index.html";

MenuItems[1] = new Object();
MenuItems[1].Text = "Browser TV";
MenuItems[1].Link = "browsertv.html";

MenuItems[2] = new Object();
MenuItems[2].Text = "Stream Status";
MenuItems[2].Link = "streamstatus.html";

MenuItems[3] = new Object();
MenuItems[3].Text = "Tuner Status";
MenuItems[3].Link = "tunerstatus.html";

MenuItems[4] = new Object();
MenuItems[4].Text = "Unicable Settings";
MenuItems[4].Link = "scif.html";

MenuItems[5] = new Object();
MenuItems[5].Text = "LNB Settings";
MenuItems[5].Link = "lnbsettings.html";

MenuItems[6] = new Object();
MenuItems[6].Text = "System Settings";
MenuItems[6].Link = "system.html";

MenuItems[7] = new Object();
MenuItems[7].Text = "Multicast Setup";
MenuItems[7].Link = "multicast.html";

MenuItems[8] = new Object();
MenuItems[8].Text = "Channel Lists";
MenuItems[8].Link = "channellists.html";

MenuItems[9] = new Object();
MenuItems[9].Text = "Update";
MenuItems[9].Link = "update.html";

MenuItems[10] = new Object();
MenuItems[10].Text = "Reboot";
MenuItems[10].Link = "reboot.html";

MenuItems[11] = new Object();
MenuItems[11].Text = "Hardware Monitor";
MenuItems[11].Link = "monitor.html";

MenuItems[12] = new Object();
MenuItems[12].Text = "Licenses";
MenuItems[12].Link = "licenses.html";

// add additional items here

// ---------------------------------------------------------------
// Don't touch

function CreateMenu()
{
  document.write('<table class="menutable">');
  for(i = 0; i < MenuItems.length; i++ )
  {
    if( document.URL.indexOf(MenuItems[i].Link) < 0 )
      document.write('<tr><td><a href="/'+MenuItems[i].Link+'">'+MenuItems[i].Text+'</a></td></tr>');
    else
      document.write('<tr><td class="menucur">'+MenuItems[i].Text+'</td></tr>');
  }
  document.write('</table>');
}

// --------------------------------------------------------------

var browserType = "unk";
var browserLanguage = "en";
var browserPlatform = "unk";

if( navigator.appVersion.indexOf("MSIE") >= 0 )
  browserType = "MSIE";
else
  browserType = "Netscape";

if( browserType == "MSIE" )
  browserLanguage = navigator.browserLanguage.substr(0,2);
else if( browserType == "Netscape" )
{
  var nplat = navigator.platform.toLowerCase();
  browserLanguage = navigator.language.substr(0,2);
  if( nplat.indexOf("win") == 0 )
    browserPlatform = "win";
  else if( nplat.indexOf("linux") >= 0 )
    browserPlatform = "linux";
  else if( nplat.indexOf("ipad") >= 0 )
    browserPlatform = "ipad";
}

// document.write(navigator.appName);
// document.write(navigator.appVersion);
// document.write(navigator.browserLanguage);
// document.write(navigator.language);
// document.write(navigator.platform);
// document.write(navigator.mimeTypes.length);
// document.write(navigator.plugins.length);
// document.write(window.ActiveXObject ? "AX" : "no AX");
