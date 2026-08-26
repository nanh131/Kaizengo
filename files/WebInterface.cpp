#include "WebInterface.h"

#include <WiFi.h>
#include <string.h>

namespace {
const char* const kUploadTempPath = "/.cnc-upload.tmp";

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="vi"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>CNC Bridge</title>
<style>
:root{color-scheme:dark;--bg:#09111d;--panel:#111d2d;--line:#26374e;--text:#e5edf8;--muted:#94a9c3;--ok:#2dd4a0;--warn:#fbbf24;--bad:#fb7185;--blue:#50a7ff}*{box-sizing:border-box}body{margin:0;font:15px system-ui,-apple-system,Segoe UI,sans-serif;background:radial-gradient(circle at top right,#163258,#09111d 48%);color:var(--text)}main{max-width:1080px;margin:auto;padding:24px}h1{margin:0;font-size:26px}.sub{color:var(--muted);margin:.35rem 0 1.4rem}.grid{display:grid;grid-template-columns:1.25fr .75fr;gap:16px}.card{background:#111d2de8;border:1px solid var(--line);border-radius:14px;padding:18px;box-shadow:0 12px 28px #02060b55}.card h2{font-size:16px;margin:0 0 13px}.status{font-weight:700;text-transform:uppercase;letter-spacing:.06em}.idle,.complete{color:var(--ok)}.running{color:var(--blue)}.paused,.finishing{color:var(--warn)}.failed,.aborted{color:var(--bad)}.kv{display:grid;grid-template-columns:142px 1fr;gap:8px 12px;align-items:baseline}.kv span{color:var(--muted)}code{font:13px ui-monospace,SFMono-Regular,Consolas,monospace;word-break:break-word}button,input,select{font:inherit;border-radius:8px;border:1px solid var(--line);padding:9px 11px;background:#0a1422;color:var(--text)}button{cursor:pointer;background:#1769ba;border-color:#2d7bc5;font-weight:650}button:hover{filter:brightness(1.12)}button.secondary{background:#243449;border-color:#344d69}button.warn{background:#a55d09;border-color:#d18a22}button.danger{background:#9f2337;border-color:#d9435b}button:disabled{opacity:.5;cursor:not-allowed}.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:15px}.upload{display:flex;gap:8px;align-items:center;flex-wrap:wrap}.upload input{min-width:230px}table{width:100%;border-collapse:collapse;margin-top:13px}th,td{padding:9px 6px;border-bottom:1px solid var(--line);text-align:left}th{font-size:12px;color:var(--muted);text-transform:uppercase}td:last-child{text-align:right}.file-actions{display:flex;justify-content:flex-end;gap:6px}.file-actions button{padding:5px 8px;font-size:13px}.inputs{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}.input{padding:10px 6px;background:#0b1624;border:1px solid var(--line);border-radius:8px;text-align:center;color:var(--muted)}.input.active{color:#081a15;background:var(--ok);border-color:#6bf0c5;font-weight:700}.notice{margin-top:16px;padding:10px 12px;border-left:3px solid var(--warn);background:#33270f;color:#f8dc97;border-radius:4px}.message{min-height:22px;margin-top:10px;color:var(--muted)}.message.error{color:var(--bad)}@media(max-width:760px){main{padding:15px}.grid{grid-template-columns:1fr}.kv{grid-template-columns:106px 1fr}}
</style></head><body><main><h1>CNC Bridge</h1><p class="sub">G-code qua Wi-Fi · ESP32-S3 · GRBL UART</p>
<div class="grid"><section class="card"><h2>Trạng thái công việc</h2><div class="kv"><span>Trạng thái</span><strong id="state" class="status">Đang tải…</strong><span>Tệp đang chọn</span><code id="jobFile">—</code><span>Dòng nguồn / đã gửi</span><code id="lines">—</code><span>Trạng thái GRBL</span><code id="controller">—</code><span>Phản hồi CNC</span><code id="reply">—</code><span>Mạng</span><code id="network">—</code></div><div class="actions"><button id="pause" class="warn">Tạm dừng</button><button id="resume" class="secondary">Tiếp tục</button><button id="abort" class="danger">Hủy / Reset mềm</button></div><p class="notice">Dừng khẩn phải là mạch dây cứng độc lập. Nút “Hủy” chỉ gửi feed-hold và reset mềm GRBL.</p></section>
<section class="card"><h2>Ngõ vào cách ly</h2><div class="inputs"><div class="input" id="in0">IN 1</div><div class="input" id="in1">IN 2</div><div class="input" id="in2">IN 3</div><div class="input" id="in3">IN 4</div></div><div class="message" id="message"></div></section></div>
<section class="card" style="margin-top:16px"><h2>Nạp G-code vào thẻ SD</h2><form id="uploadForm" class="upload"><input id="uploadFile" type="file" accept=".nc,.gcode,.tap,.txt" required><button>Nạp tệp</button></form><div class="message" id="uploadMessage"></div><table><thead><tr><th>Tệp</th><th>Dung lượng</th><th>Thao tác</th></tr></thead><tbody id="files"><tr><td colspan="3">Đang tải…</td></tr></tbody></table></section>
</main><script>
const el=id=>document.getElementById(id);let selected='';
function msg(text,bad=false,target='message'){const x=el(target);x.textContent=text||'';x.className='message'+(bad?' error':'')}
async function api(url,opts){const r=await fetch(url,opts);const t=await r.text();let data={};try{data=t?JSON.parse(t):{}}catch(_){throw Error('Phản hồi máy chủ không hợp lệ')}if(!r.ok)throw Error(data.error||('HTTP '+r.status));return data}
function formatSize(n){return n<1024?n+' B':n<1048576?(n/1024).toFixed(1)+' KB':(n/1048576).toFixed(2)+' MB'}
async function update(){try{const d=await api('/api/status');const j=d.job;const s=el('state');s.textContent=j.state;s.className='status '+j.state;el('jobFile').textContent=j.file||'—';el('lines').textContent=j.sourceLine+' / '+j.sentLine+(j.waitingForAck?' (đợi ok)':'');el('controller').textContent=j.controllerState||'—';el('reply').textContent=j.lastResponse||'—';el('network').textContent=d.network.mode+' · '+d.network.ip;d.inputs.forEach((v,i)=>el('in'+i).classList.toggle('active',v));el('pause').disabled=j.state!=='running';el('resume').disabled=j.state!=='paused';el('abort').disabled=!['running','paused','finishing'].includes(j.state);if(!d.sdReady)msg('Không tìm thấy SD — không thể nạp hoặc chạy G-code.',true)}catch(e){msg(e.message,true)}}
async function files(){try{const d=await api('/api/files');const body=el('files');body.replaceChildren();if(!d.files.length){body.innerHTML='<tr><td colspan="3">Chưa có tệp G-code.</td></tr>';return}d.files.forEach(f=>{const tr=document.createElement('tr'),a=document.createElement('td'),b=document.createElement('td'),c=document.createElement('td'),start=document.createElement('button'),del=document.createElement('button');a.textContent=f.name;b.textContent=formatSize(f.size);start.textContent='Chạy';del.textContent='Xóa';del.className='danger';start.onclick=()=>startJob(f.name);del.onclick=()=>deleteFile(f.name);c.className='file-actions';c.append(start,del);tr.append(a,b,c);body.append(tr)})}catch(e){msg(e.message,true)}}
async function post(url,data){return api(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(data||{})})}
async function startJob(name){if(!confirm('Bắt đầu chạy '+name+'? Hãy chắc chắn máy đã homing, tọa độ và dao đều đúng.'))return;try{await post('/api/job/start',{file:name});msg('Đã gửi lệnh bắt đầu: '+name);update()}catch(e){msg(e.message,true)}}
async function deleteFile(name){if(!confirm('Xóa '+name+' khỏi thẻ SD?'))return;try{await post('/api/file/delete',{file:name});files()}catch(e){msg(e.message,true)}}
el('pause').onclick=async()=>{try{await post('/api/job/pause');update()}catch(e){msg(e.message,true)}};el('resume').onclick=async()=>{try{await post('/api/job/resume');update()}catch(e){msg(e.message,true)}};el('abort').onclick=async()=>{if(!confirm('Hủy công việc và gửi reset mềm GRBL?'))return;try{await post('/api/job/abort');update()}catch(e){msg(e.message,true)}};
el('uploadForm').onsubmit=async e=>{e.preventDefault();const f=el('uploadFile').files[0];if(!f)return;msg('Đang nạp '+f.name+'…',false,'uploadMessage');try{const d=await api('/api/upload',{method:'POST',body:new FormData(e.target)});msg('Đã nạp '+d.file,false,'uploadMessage');e.target.reset();files()}catch(err){msg(err.message,true,'uploadMessage')}};
files();update();setInterval(update,1000);
</script></body></html>
)HTML";
}  // namespace

WebInterface::WebInterface(WebServer& server, GCodeStreamer& streamer,
                           IsolatedInputs& inputs, SDLogger& logger)
    : _server(server),
      _streamer(streamer),
      _inputs(inputs),
      _logger(logger),
      _usingFallbackAp(false),
      _uploadBytes(0),
      _uploadFailed(false),
      _uploadRejected(false) {}

void WebInterface::begin(bool usingFallbackAp) {
  _usingFallbackAp = usingFallbackAp;
  _server.on("/", HTTP_GET, [this]() { serveIndex(); });
  _server.on("/api/status", HTTP_GET, [this]() { sendStatus(); });
  _server.on("/api/files", HTTP_GET, [this]() { sendFileList(); });
  _server.on("/api/upload", HTTP_POST, [this]() { finishUpload(); },
             [this]() { handleUpload(); });
  _server.on("/api/job/start", HTTP_POST, [this]() { startJob(); });
  _server.on("/api/job/pause", HTTP_POST, [this]() { pauseJob(); });
  _server.on("/api/job/resume", HTTP_POST, [this]() { resumeJob(); });
  _server.on("/api/job/abort", HTTP_POST, [this]() { abortJob(); });
  _server.on("/api/file/delete", HTTP_POST, [this]() { deleteFile(); });
  _server.onNotFound([this]() { notFound(); });
  _server.begin();
  Serial.println(F("[Web] HTTP dashboard started."));
}

void WebInterface::handleClient() {
  _server.handleClient();
}

bool WebInterface::isAuthenticated() const {
  return _server.authenticate(WEB_USERNAME, WEB_PASSWORD);
}

bool WebInterface::credentialsConfigured() const {
  return strcmp(WEB_PASSWORD, "CHANGE_ME_WEB_PASSWORD") != 0 &&
         strlen(WEB_PASSWORD) >= 12;
}

bool WebInterface::authorize() {
  if (!credentialsConfigured()) {
    sendError(503, "Set a unique WEB_PASSWORD of at least 12 characters in config.h");
    return false;
  }
  if (isAuthenticated()) {
    return true;
  }
  _server.requestAuthentication();
  return false;
}

bool WebInterface::requireSd() {
  if (!_logger.isReady()) {
    sendError(503, "SD card is unavailable");
    return false;
  }
  return true;
}

void WebInterface::sendJson(int statusCode, const String& body) {
  _server.sendHeader("Cache-Control", "no-store");
  _server.send(statusCode, "application/json; charset=utf-8", body);
}

void WebInterface::sendError(int statusCode, const char* message) {
  String body = "{\"error\":";
  body += jsonString(String(message == nullptr ? "Unknown error" : message));
  body += "}";
  sendJson(statusCode, body);
}

void WebInterface::serveIndex() {
  if (!authorize()) {
    return;
  }
  _server.sendHeader("Cache-Control", "no-store");
  _server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void WebInterface::sendStatus() {
  if (!authorize()) {
    return;
  }
  const IPAddress ip = _usingFallbackAp ? WiFi.softAPIP() : WiFi.localIP();
  String body = "{\"sdReady\":";
  body += _logger.isReady() ? "true" : "false";
  body += ",\"network\":{\"mode\":";
  body += _usingFallbackAp ? "access_point" : "station";
  body += "\",\"ip\":";
  body += jsonString(ip.toString());
  body += "},\"inputs\":[";
  for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
    if (i > 0) {
      body += ',';
    }
    body += _inputs.isActive(i) ? "true" : "false";
  }
  body += "],\"job\":";
  body += _streamer.statusJson();
  body += "}";
  sendJson(200, body);
}

void WebInterface::sendFileList() {
  if (!authorize()) {
    return;
  }
  if (!requireSd()) {
    return;
  }

  String body = "{\"files\":[";
  bool first = true;
  File directory = SD.open("/");
  if (directory) {
    for (File entry = directory.openNextFile(); entry;
         entry = directory.openNextFile()) {
      if (!entry.isDirectory()) {
        String path = entry.name();
        String safePath;
        if (makeSafeGCodePath(path, safePath)) {
          if (!first) {
            body += ',';
          }
          first = false;
          String displayName = safePath.substring(1);
          body += "{\"name\":";
          body += jsonString(displayName);
          body += ",\"size\":";
          body += String(entry.size());
          body += "}";
        }
      }
      entry.close();
    }
    directory.close();
  }
  body += "]}";
  sendJson(200, body);
}

void WebInterface::handleUpload() {
  HTTPUpload& upload = _server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    _uploadBytes = 0;
    _uploadFailed = false;
    _uploadRejected = !credentialsConfigured() || !isAuthenticated();
    _uploadError = "";
    _uploadTarget = "";
    if (_uploadRejected) {
      _uploadError = "Authentication required";
      return;
    }
    if (!_logger.isReady()) {
      _uploadFailed = true;
      _uploadError = "SD card is unavailable";
      return;
    }
    if (!makeSafeGCodePath(upload.filename, _uploadTarget)) {
      _uploadFailed = true;
      _uploadError = "Only safe .nc, .gcode, .tap, or .txt filenames are accepted";
      return;
    }
    if (strcmp(_streamer.fileName(), _uploadTarget.c_str()) == 0 && _streamer.isBusy()) {
      _uploadFailed = true;
      _uploadError = "Cannot replace the file that is currently running";
      return;
    }
    SD.remove(kUploadTempPath);
    _uploadFile = SD.open(kUploadTempPath, FILE_WRITE);
    if (!_uploadFile) {
      _uploadFailed = true;
      _uploadError = "Cannot create upload file on SD";
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (_uploadRejected || _uploadFailed) {
      return;
    }
    if (_uploadBytes + upload.currentSize > MAX_UPLOAD_BYTES) {
      _uploadFailed = true;
      _uploadError = "Upload exceeds configured size limit";
      _uploadFile.close();
      SD.remove(kUploadTempPath);
      return;
    }
    if (_uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
      _uploadFailed = true;
      _uploadError = "SD write failed during upload";
      _uploadFile.close();
      SD.remove(kUploadTempPath);
      return;
    }
    _uploadBytes += upload.currentSize;
    return;
  }

  if (upload.status == UPLOAD_FILE_END && !_uploadRejected && !_uploadFailed) {
    _uploadFile.close();
    SD.remove(_uploadTarget);
    if (!SD.rename(kUploadTempPath, _uploadTarget)) {
      _uploadFailed = true;
      _uploadError = "Unable to finalize uploaded file";
      SD.remove(kUploadTempPath);
    }
  }

  if (upload.status == UPLOAD_FILE_ABORTED) {
    if (_uploadFile) {
      _uploadFile.close();
    }
    SD.remove(kUploadTempPath);
    _uploadFailed = true;
    _uploadError = "Upload aborted";
  }
}

void WebInterface::finishUpload() {
  if (_uploadRejected) {
    if (!credentialsConfigured()) {
      sendError(503, "Set a unique WEB_PASSWORD of at least 12 characters in config.h");
      return;
    }
    _server.requestAuthentication();
    return;
  }
  if (_uploadFailed) {
    sendError(400, _uploadError.c_str());
    return;
  }
  String body = "{\"ok\":true,\"file\":";
  body += jsonString(_uploadTarget.substring(1));
  body += "}";
  sendJson(201, body);
}

void WebInterface::startJob() {
  if (!authorize()) {
    return;
  }
  if (!requireSd()) {
    return;
  }
  if (!_server.hasArg("file")) {
    sendError(400, "Missing file parameter");
    return;
  }
  String path;
  if (!makeSafeGCodePath(_server.arg("file"), path)) {
    sendError(400, "Invalid G-code filename");
    return;
  }
  if (!_streamer.start(path.c_str())) {
    sendError(409, _streamer.lastResponse());
    return;
  }
  sendJson(200, "{\"ok\":true}");
}

void WebInterface::deleteFile() {
  if (!authorize()) {
    return;
  }
  if (!requireSd()) {
    return;
  }
  if (!_server.hasArg("file")) {
    sendError(400, "Missing file parameter");
    return;
  }
  String path;
  if (!makeSafeGCodePath(_server.arg("file"), path)) {
    sendError(400, "Invalid G-code filename");
    return;
  }
  if (_streamer.isBusy() && strcmp(path.c_str(), _streamer.fileName()) == 0) {
    sendError(409, "Cannot delete the active job");
    return;
  }
  if (!SD.exists(path) || !SD.remove(path)) {
    sendError(404, "File not found or cannot be deleted");
    return;
  }
  sendJson(200, "{\"ok\":true}");
}

void WebInterface::pauseJob() {
  if (!authorize()) {
    return;
  }
  if (!_streamer.pause()) {
    sendError(409, "No running job to pause");
    return;
  }
  sendJson(200, "{\"ok\":true}");
}

void WebInterface::resumeJob() {
  if (!authorize()) {
    return;
  }
  if (!_streamer.resume()) {
    sendError(409, "No paused job to resume");
    return;
  }
  sendJson(200, "{\"ok\":true}");
}

void WebInterface::abortJob() {
  if (!authorize()) {
    return;
  }
  if (!_streamer.abort()) {
    sendError(409, "No active job to abort");
    return;
  }
  sendJson(200, "{\"ok\":true}");
}

void WebInterface::notFound() {
  if (!authorize()) {
    return;
  }
  sendError(404, "Endpoint not found");
}

bool WebInterface::makeSafeGCodePath(const String& value, String& path) const {
  String name = value;
  name.replace("\\", "/");
  const int separator = name.lastIndexOf('/');
  if (separator >= 0) {
    name = name.substring(separator + 1);
  }
  if (name.length() == 0 || name.length() > 63 || name == "." || name == ".." ||
      !hasGCodeExtension(name)) {
    return false;
  }
  for (size_t i = 0; i < name.length(); ++i) {
    const char c = name.charAt(i);
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) {
      return false;
    }
  }
  path = "/" + name;
  return true;
}

bool WebInterface::hasGCodeExtension(const String& name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".nc") || lower.endsWith(".gcode") ||
         lower.endsWith(".tap") || lower.endsWith(".txt");
}

String WebInterface::jsonString(const String& value) {
  String result = "\"";
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value.charAt(i);
    if (c == '\\' || c == '\"') {
      result += '\\';
    }
    result += (static_cast<unsigned char>(c) < 32) ? ' ' : c;
  }
  result += "\"";
  return result;
}
