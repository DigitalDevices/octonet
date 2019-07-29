
//

MenuItems = new Array();

MenuItems.push( { Text:"Home", Link:"index.html" } );
//MenuItems.push( { Text:"Browser TV", Link:"browsertv.html" } );
MenuItems.push( { Text:"EPG", Link:"epg.html" } );
MenuItems.push( { Text:"Stream Status", Link:"streamstatus.html" } );
MenuItems.push( { Text:"Tuner Status", Link:"tunerstatus.html" } );
MenuItems.push( { Text:"Unicable Settings", Link:"scif.html" } );
MenuItems.push( { Text:"LNB Settings", Link:"lnbsettings.html" } );
MenuItems.push( { Text:"System Settings", Link:"system.html" } );
MenuItems.push( { Text:"Multicast Setup", Link:"multicast.html" } );
MenuItems.push( { Text:"Channel Lists", Link:"channellists.html" } );
MenuItems.push( { Text:"Update", Link:"update.html" } );
MenuItems.push( { Text:"Reboot", Link:"reboot.html" } );
MenuItems.push( { Text:"Hardware Monitor", Link:"monitor.html" } );
MenuItems.push( { Text:"Licenses", Link:"licenses.html" } );

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
