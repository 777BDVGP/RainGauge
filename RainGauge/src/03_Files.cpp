// =============================================================================
// 03_Files.cpp — SPIFFS Initialisation & Web File-Manager Route Handlers
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// -----------------------------------------------------------------------------
// InitFileSystem
// Mounts SPIFFS, formats on first boot, then creates all runtime data objects.
// -----------------------------------------------------------------------------
void InitFileSystem(){

	File f;

	SPIFFS.begin();

	if (!SPIFFS.exists(StrFormatComplete)){
		FSInitLog += "Please wait 30 secs for SPIFFS to be formatted.....\n";
		if (SPIFFS.format()){
			SPIFFS.begin();
			FSInitLog += "SPIFFS formatted successfully\n";

			f = SPIFFS.open(StrFormatComplete, "w");
			if (!f){
				FSInitLog += "ERROR: Could not create FormatComplete.txt\n";
			}
			else{
				f.println("Deleting this file will cause SPIFFS to be reformatted on the next boot.");
				f.close();
				FSInitLog += "FormatComplete.txt created\n";
			}
		}
		else{
			FSInitLog += "ERROR: SPIFFS format failed\n";
		}
	}
	else{
		FSInitLog += "File System mounted successfully\n";
	}

	CreateAllParams();        // System parameters (12_Settings.cpp)
	CreateSimulation();       // Simulation mode settings (18_Simulation.cpp)
	CreateNtfyParams();       // NTFY push notification settings (13_Ntfy.cpp)
	WaterBalanceLoad();       // Soil-water bucket state (20_WaterBalance.cpp)
	TempHistoryLoad();        // Today's min/max + temperature records (21_TempHistory.cpp)
	MapHomePage();            // Home page live values (08_Home.cpp)

	// The S2 Mini has 2 MB PSRAM — keep the 28 KB page buffer out of DRAM.
	// Fall back to DRAM only if PSRAM is missing (e.g. an S2 Mini clone without it).
	HtmlBuf = (char*)ps_malloc(HTML_BUF_SIZE);
	if (!HtmlBuf) HtmlBuf = (char*)malloc(HTML_BUF_SIZE);
	if (HtmlBuf) HtmlBuf[0] = '\0';
	else FSInitLog += "ERROR: malloc failed for HtmlBuf\n";
}

// =============================================================================
// SPIFFS Web File Manager — Route Handlers
// =============================================================================

void FilesPage(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_07);
	appendMainPageSel(PS_FILES);
	appendHtmlStr(webScriptTag);

	appendHtmlStr("<div id='fsinfo' style='padding:4px 8px 2px'>"
	              "<p style='color:#475569;font-size:12px;padding:4px 0'>Loading&hellip;</p></div>");

	appendHtmlStr("<div style='display:flex;align-items:center;justify-content:space-between;"
	              "padding:4px 8px 2px'>"
	              "<b style='font-size:13px;color:#0f172a'>Files on SPIFFS</b>"
	              "<button onclick='loadFiles()' style='background:#0284c7;border:none;color:#fff;"
	              "padding:4px 10px;font-size:12px;border-radius:4px;cursor:pointer'>"
	              "&#8635; Refresh</button></div>");

	appendHtmlStr("<div id='ftbl'>"
	              "<p style='color:#475569;padding:10px;font-size:13px'>Loading&hellip;</p></div>");

	appendHtmlStr("<div id='vpanel' style='display:none;margin:4px 0'>");
	appendHtmlStr("<div style='display:flex;align-items:center;justify-content:space-between;"
	              "background:#e2e8f0;padding:4px 8px;border-radius:4px 4px 0 0'>"
	              "<span id='vfname' style='font-family:monospace;font-size:11px;color:#0f172a'></span>"
	              "<button onclick='closeView()' style='background:#dc2626;border:none;color:#fff;"
	              "padding:3px 8px;font-size:12px;border-radius:4px;cursor:pointer'>"
	              "&#10005; Close</button></div>");
	appendHtmlStr("<pre id='vcontent' style='max-height:320px;overflow:auto;background:#f8fafc;"
	              "border:1px solid #e2e8f0;border-radius:0 0 4px 4px;padding:8px;"
	              "font-size:11px;white-space:pre-wrap;word-break:break-all;margin:0'></pre>");
	appendHtmlStr("</div>");

	appendHtmlStr("<div style='padding:8px;margin-top:4px'>"
	              "<b style='font-size:13px;color:#0f172a'>Upload File</b><br><br>"
	              "<form method='POST' action='/fupload' enctype='multipart/form-data'>"
	              "<input type='file' name='fupload' "
	              "style='margin-bottom:10px;display:block;width:100%;font-size:13px'>"
	              "<button class='btn btn-bl' type='submit'>&#8679; Upload</button>"
	              "</form></div>");

	appendHtmlStr(R"JS(<script>
function fmtSz(b){if(b<1024)return b+' B';if(b<1048576)return (b/1024).toFixed(1)+' KB';return (b/1048576).toFixed(1)+' MB';}
function loadFiles(){
  fetch('/api/files').then(function(r){return r.json();}).then(function(j){
    var pct=Math.round(j.used/j.total*100);
    document.getElementById('fsinfo').innerHTML=
      "<div style='padding:4px 8px 2px;font-size:12px;color:#475569'>Used: "+fmtSz(j.used)+" of "+fmtSz(j.total)+" ("+pct+"%)</div>"
      +"<div style='background:#e2e8f0;border-radius:4px;height:6px;margin:0 8px 8px'>"
      +"<div style='background:#0284c7;height:6px;border-radius:4px;width:"+pct+"%'></div></div>";
    if(!j.files||j.files.length===0){document.getElementById('ftbl').innerHTML="<p style='color:#475569;padding:10px;font-size:13px'>No files.</p>";return;}
    function extOf(n){var i=n.lastIndexOf('.');return i<0?'':n.substring(i+1).toLowerCase();}
    function nameColor(n){var e=extOf(n);return e==='json'?'#dc2626':e==='txt'?'#0f172a':'#15803d';}
    j.files.sort(function(a,b){var ea=extOf(a.name),eb=extOf(b.name);return ea<eb?-1:ea>eb?1:a.name<b.name?-1:a.name>b.name?1:0;});
    var h='<table><tr class="hd"><td>File Name</td><td style="text-align:right">Size</td><td></td></tr>';
    j.files.forEach(function(f){
      var enc=encodeURIComponent(f.name);
      var col=nameColor(f.name);
      h+='<tr><td style="font-family:monospace;font-size:12px;color:'+col+'">'+f.name+'</td>'
        +'<td style="font-size:12px;color:#64748b;white-space:nowrap;text-align:right">'+fmtSz(f.size)+'</td>'
        +'<td style="white-space:nowrap;padding-right:0;text-align:right">'
        +'<a href="/download?f_File='+enc+'" style="display:inline-block;padding:3px 8px;background:#0284c7;color:#fff;border-radius:4px;font-size:11px;text-decoration:none;margin-right:3px">&#8595;&nbsp;DL</a>'
        +'<button onclick="viewFile(\''+f.name+'\')" style="padding:3px 8px;background:#475569;color:#fff;border:none;border-radius:4px;font-size:11px;cursor:pointer;margin-right:3px">&#128065;&nbsp;View</button>'
        +'<button onclick="delFile(\''+f.name+'\')" style="padding:3px 8px;background:#dc2626;color:#fff;border:none;border-radius:4px;font-size:11px;cursor:pointer">&#10005;&nbsp;Del</button>'
        +'</td></tr>';
    });
    h+='</table>';
    document.getElementById('ftbl').innerHTML=h;
  }).catch(function(){document.getElementById('ftbl').innerHTML="<p style='color:#dc2626;padding:10px;font-size:13px'>Failed to load.</p>";});
}
function delFile(name){
  if(!confirm('Delete '+name+'?\nThis cannot be undone.'))return;
  fetch('/files/delete',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'f='+encodeURIComponent(name)})
    .then(function(r){return r.json();})
    .then(function(j){if(j.ok)loadFiles();else alert('Delete error: '+j.msg);})
    .catch(function(e){alert('Delete failed: '+e);});
}
function viewFile(name){
  document.getElementById('vcontent').textContent='Loading…';
  document.getElementById('vfname').textContent=name;
  document.getElementById('vpanel').style.display='block';
  document.getElementById('vpanel').scrollIntoView({behavior:'smooth'});
  fetch('/files/view?f='+encodeURIComponent(name))
    .then(function(r){return r.text();})
    .then(function(t){document.getElementById('vcontent').textContent=t;})
    .catch(function(){document.getElementById('vcontent').textContent='(error loading file)';});
}
function closeView(){document.getElementById('vpanel').style.display='none';document.getElementById('vcontent').textContent='';}
loadFiles();
</script>)JS");

	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

void handleApiFiles(AsyncWebServerRequest *request) {
	DynamicJsonDocument doc(4096);
	doc["used"]  = (size_t)SPIFFS.usedBytes();
	doc["total"] = (size_t)SPIFFS.totalBytes();
	JsonArray arr = doc.createNestedArray("files");
	File root = SPIFFS.open("/");
	File f    = root.openNextFile();
	while (f) {
		JsonObject o = arr.createNestedObject();
		String fname = String(f.name());
		if (fname.startsWith("/")) fname = fname.substring(1);
		if (IsProtectedFile(fname)){ arr.remove(arr.size() - 1); f = root.openNextFile(); continue; }
		o["name"] = fname;
		o["size"] = (size_t)f.size();
		f = root.openNextFile();
	}
	String json;
	serializeJson(doc, json);
	request->send(200, "application/json", json);
}

void handleFileView(AsyncWebServerRequest *request) {
	if (!request->hasArg("f")) { request->send(400, "text/plain", "Missing filename"); return; }
	String path = "/" + request->arg("f");
	if (IsProtectedFile(path)) { request->send(403, "text/plain", "This file holds the WiFi password and is not shown"); return; }
	if (!SPIFFS.exists(path)) { request->send(404, "text/plain", "Not found"); return; }
	request->send(SPIFFS, path, "text/plain");
}

void handleFileDeleteJson(AsyncWebServerRequest *request) {
	if (!request->hasArg("f")) {
		request->send(200, "application/json", "{\"ok\":false,\"msg\":\"missing param\"}");
		return;
	}
	String target = request->arg("f");
	if (!target.startsWith("/")) target = "/" + target;

	String exactPath = "";
	{
		File root = SPIFFS.open("/");
		File entry = root.openNextFile();
		while (entry) {
			String stored = String(entry.name());
			if (!stored.startsWith("/")) stored = "/" + stored;
			if (stored == target) { exactPath = stored; break; }
			entry = root.openNextFile();
		}
	}

	if (exactPath.length() == 0) {
		String j = "{\"ok\":false,\"msg\":\"not found: " + target + "\"}";
		request->send(200, "application/json", j);
		return;
	}
	if (SPIFFS.remove(exactPath)) {
		request->send(200, "application/json", "{\"ok\":true}");
	} else {
		request->send(200, "application/json", "{\"ok\":false,\"msg\":\"remove failed\"}");
	}
}

// GET /download?f_File=<name> — the DL button on the FILES page.
void File_Download(AsyncWebServerRequest *request){
	if (!request->hasParam("f_File")) { request->redirect("/files"); return; }
	String path = "/" + request->getParam("f_File")->value();
	if (IsProtectedFile(path)) { request->send(403, "text/plain", "This file holds the WiFi password and is not downloaded"); return; }
	if (SPIFFS.exists(path)) request->send(SPIFFS, path, "application/octet-stream", true);
	else                     request->send(404, "text/plain", "File not found");
}

File UploadFile;
void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
	if (index == 0){
		if (!filename.startsWith("/")) filename = "/" + filename;
		SPIFFS.remove(filename);
		UploadFile = SPIFFS.open(filename, "w");
	}

	if (UploadFile) UploadFile.write(data, len);

	if (final){
		if (UploadFile){
			UploadFile.close();
			SetMess    = FA_MESS_GRN;
			SpesialStr = "File "; SpesialStr += filename; SpesialStr += " uploaded successfully";
		}
		else{
			SetMess    = FA_MESS_RED;
			SpesialStr = "File upload failed";
		}
	}
}
