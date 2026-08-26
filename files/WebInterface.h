#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>
#include <WebServer.h>
#include "GCodeStreamer.h"
#include "IsolatedInputs.h"
#include "SDLogger.h"

// Local, password-protected dashboard and REST API.  It intentionally exposes
// no unauthenticated endpoint and has no cloud dependency.
class WebInterface {
 public:
  WebInterface(WebServer& server, GCodeStreamer& streamer,
               IsolatedInputs& inputs, SDLogger& logger);

  void begin(bool usingFallbackAp);
  void handleClient();

 private:
  WebServer& _server;
  GCodeStreamer& _streamer;
  IsolatedInputs& _inputs;
  SDLogger& _logger;
  bool _usingFallbackAp;

  File _uploadFile;
  String _uploadTarget;
  size_t _uploadBytes;
  bool _uploadFailed;
  bool _uploadRejected;
  String _uploadError;

  bool authorize();
  bool isAuthenticated() const;
  bool credentialsConfigured() const;
  bool requireSd();
  void sendJson(int statusCode, const String& body);
  void sendError(int statusCode, const char* message);
  void serveIndex();
  void sendStatus();
  void sendFileList();
  void handleUpload();
  void finishUpload();
  void startJob();
  void deleteFile();
  void pauseJob();
  void resumeJob();
  void abortJob();
  void notFound();

  bool makeSafeGCodePath(const String& value, String& path) const;
  static bool hasGCodeExtension(const String& name);
  static String jsonString(const String& value);
};

#endif  // WEB_INTERFACE_H
