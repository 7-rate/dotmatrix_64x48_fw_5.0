#include <WebServer.h>
#include <StreamString.h>
#include "spiffs_fs.h"
#include "mz_update.h"


static WebServer server(80);


 
static String updateIndex = // TODO: Error handling
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

static bool send_common_header()
{
	/*
	if(!in_recovery)
	{
		if(!server.authenticate(user_name, password.c_str()))
		{
			server.requestAuthentication();
			return false;
		}
	}
	return true;
	*/
	return true;
}

static bool loadFromFS(String path){
	String dataType = F("text/plain");
	if(path.endsWith("/")) path += F("index.html");

	if(path.endsWith(".htm") || path.endsWith(".html")) dataType = F("text/html");
	else if(path.endsWith(".css")) dataType = F("text/css");
	else if(path.endsWith(".js"))  dataType = F("application/javascript");
	else if(path.endsWith(".png")) dataType = F("image/png");
	else if(path.endsWith(".gif")) dataType = F("image/gif");
	else if(path.endsWith(".jpg")) dataType = F("image/jpeg");
	else if(path.endsWith(".ico")) dataType = F("image/x-icon");
	else if(path.endsWith(".xml")) dataType = F("text/xml");
	else if(path.endsWith(".pdf")) dataType = F("application/pdf");
	else if(path.endsWith(".zip")) dataType = F("application/zip");
	else if(path.endsWith(".txt")) dataType = F("text/plain");

	path = String(F("/w")) + path; // all contents must be under "w" directory
		// TODO: path reverse-traversal check

	if(FS.exists(path + F(".gz")))
      path += String(F(".gz")); // handle gz

	if (!FS.exists(path))
	{
		printf("Requested load form '%s' but file does not exist.\n", path.c_str());
		return false;
	}

	File dataFile = FS.open(path.c_str(), "r");


	server.streamFile(dataFile, dataType);

	dataFile.close();
	return true;
}


static void handleNotFound()
{
	if(!send_common_header()) return;
	if(loadFromFS(server.uri())) return;

	if(server.uri() == "/")
	{
		// filesystem content missing or filesystem mount failed.
		// show fallback message
		printf("SPIFFS content missing or mount failed. Showing fallback message.\n");
		server.send(200, F("text/html"), updateIndex);
		return;
	}

	String message = F("Not Found\n\n");
	message += String(F("URI: "));
	message += server.uri();
	message += String(F("\nMethod: "));
	message += (server.method() == HTTP_GET)?F("GET"):F("POST");
	message += String(F("\nArguments: "));
	message += server.args();
	message += String(F("\n"));
	for (uint8_t i=0; i<server.args(); i++){
		message += String(F(" NAME:"))+server.argName(i) + F("\n VALUE:") + server.arg(i) + F("\n");
	}
	Serial.print(message);
	server.send(404, F("text/plain"), message);

}

static void string_json(const String &s, Stream & st)
{
	st.print((char)'"'); // starting "
	const char *p = s.c_str();
	while(*p)
	{
		char c = *p;

		if(c < 0x20)
			st.printf_P(PSTR("\\u%04d"), (int)c); // control characters
		else if(c == '\\')
			st.print(F("\\\\"));
		else if(c == '"')
			st.print(F("\\\""));
		else
			st.print(c); // other characters

		++p;
	}
	st.print((char)'"'); // ending "
}

static void web_server_export_json_for_ui(bool js)
{
	StreamString st;
	if(js) st.print(F("window.settings="));
	st.print(F("{\"result\":\"ok\",\"values\":{\n"));
/*
	string_vector v;

	v = calendar_get_ntp_server();
	st.print(F("\"cal_ntp_servers\":[\n"));
	string_json(v[0], st); st.print((char)',');
	string_json(v[1], st); st.print((char)',');
	string_json(v[2], st); st.print((char)']');

	st.print(F(",\n"));
	st.print(F("\"cal_timezone\":"));
	st.printf_P(PSTR("%d"), calendar_get_timezone());

	st.print(F(",\n"));
	st.print(F("\"admin_pass\":"));
	string_json(password, st);

	st.print(F(",\n"));
	st.print(F("\"ui_marquee\":"));
	string_json(ui_get_marquee(), st);

	st.print(F(",\n"));
	st.print(F("\"version_info\":{"));

	st.print(F("\"date\":"));
	string_json(_BuildInfo.date, st);
	st.print(F(",\n"));

	st.print(F("\"time\":"));
	string_json(_BuildInfo.time, st);
	st.print(F(",\n"));

	st.print(F("\"src_version\":"));
	string_json(_BuildInfo.src_version, st);
	st.print(F(",\n"));

	st.print(F("\"env_version\":"));
	string_json(_BuildInfo.env_version, st);

	st.print(F("}\n"));

*/
	st.print(F("}}\n"));
	if(js) st.print((char)';');

	if(js)
		server.send(200, F("application/javascript"), st);
	else
		server.send(200, F("application/json"), st);
}

void web_server_setup()
{
	// setup handlers

	// TODO: recovery mode
	server.onNotFound(handleNotFound);

	server.on(F("/settings/settings.json"), HTTP_GET, []() {
			if(!send_common_header()) return;
			web_server_export_json_for_ui(false);
		});
	server.on(F("/settings/settings.js"), HTTP_GET, []() {
			if(!send_common_header()) return;
			web_server_export_json_for_ui(true);
		});

	server.on("/update", HTTP_GET, []() {
		server.sendHeader("Connection", "close");
		server.send(200, "text/html", updateIndex);
	});

	server.on("/update", HTTP_POST, []() {
		server.sendHeader("Connection", "close");
		server.send(200, "text/plain", /*(Update.hasError()) ? "FAIL" : */"OK");
		// TODO: error handling

	  }, []() {
		HTTPUpload& upload = server.upload();
		if (upload.status == UPLOAD_FILE_START) {
			printf("Update: %s\n", upload.filename.c_str());

			Updater.begin();

		} else if (upload.status == UPLOAD_FILE_WRITE) {

			Updater.write_data(upload.buf, upload.currentSize);

		} else if (upload.status == UPLOAD_FILE_END) {

			if(Updater.finish())
			{
				reboot(); // reboot 
			}

		}
	  });

	server.onNotFound(handleNotFound);

	server.begin();
	puts("HTTP server started");

}

void web_server_handle_client()
{
	server.handleClient();
}
