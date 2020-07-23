#define ARDUINOJSON_USE_LONG_LONG 1 // for using int_64 data
#include "CTBot.h"
#include "Utilities.h"

CTBot::CTBot() {
	m_lastUpdate          = 0;  // not updated yet
	m_UTF8Encoding        = false; // no UTF8 encoded string conversion
}

CTBot::~CTBot() = default;

void CTBot::setTelegramToken(String token)
{	m_token = token;}

String CTBot::sendCommand(String command, String parameters)
{

	// must filter command + parameters from escape sequences and spaces
	const String URL = "GET /bot" + m_token + (String)"/" + command + parameters;

	// send the HTTP request
	return(m_connection.send(URL));
}

String CTBot::toUTF8(String message) const
{
	String converted("");
	uint16_t i = 0;
	while (i < message.length()) {
		String subMessage(message[i]);
		if (message[i] != '\\') {
			converted += subMessage;
			i++;
		} else {
			// found "\"
			i++;
			if (i == message.length()) {
				// no more characters
				converted += subMessage;
			} else {
				subMessage += (String)message[i];
				if (message[i] != 'u') {
					converted += subMessage;
					i++;
				} else {
					//found \u escape code
					i++;
					if (i == message.length()) {
						// no more characters
						converted += subMessage;
					} else {
						uint8_t j = 0;
						while ((j < 4) && ((j + i) < message.length())) {
							subMessage += (String)message[i + j];
							j++;
						}
						i += j;
						String utf8;
						if (unicodeToUTF8(subMessage, utf8))
							converted += utf8;
						else
							converted += subMessage;
					}
				}
			}
		}
	}
	return converted;
}

void CTBot::enableUTF8Encoding(bool value) 
{	m_UTF8Encoding = value;}

bool CTBot::testConnection(){
	TBUser user;
	return getMe(user);
}

bool CTBot::getMe(TBUser &user) {
	String response = sendCommand("getMe");
	if (response.length() == 0)
		return false;

	DynamicJsonDocument jsonDocument(CTBOT_BUFFER_SIZE);
	if(!deserializeDoc(jsonDocument, response)) return CTBotMessageNoData;

	bool ok = jsonDocument["ok"];
	if (!ok) {
		serialLog("getMe error:");
		serialLog(jsonDocument);
		serialLog("\n");
		return false;
	}

	serialLog(jsonDocument);
	serialLog("\n");

	user.id           = jsonDocument["result"]["id"]			.as<int32_t>();
	user.isBot        = jsonDocument["result"]["is_bot"]		.as<bool>();
	user.firstName    = jsonDocument["result"]["first_name"]	.as<String>();
	user.lastName     = jsonDocument["result"]["last_name"]		.as<String>();
	user.username     = jsonDocument["result"]["username"]		.as<String>();
	user.languageCode = jsonDocument["result"]["language_code"]	.as<String>();
	return true;
}

CTBotMessageType CTBot::getNewMessage(TBMessage &message) {
	char buf[21];

	message.messageType = CTBotMessageNoData;

	ltoa(m_lastUpdate, buf, 10);
	// polling timeout: add &timeout=<seconds>
	// default is zero (short polling).
	String parameters = "?limit=1&allowed_updates=message,callback_query";
	if (m_lastUpdate != 0)
		parameters += "&offset=" + (String)buf;
	String response = sendCommand("getUpdates", parameters);
	if (response.length() == 0) {
		serialLog("getNewMessage error: response with no data\n");
		return CTBotMessageNoData;
	}

	if (m_UTF8Encoding)
		response = toUTF8(response);
	
	DynamicJsonDocument jsonDocument(CTBOT_BUFFER_SIZE);	
	if(!deserializeDoc(jsonDocument, response)) return CTBotMessageNoData;

	bool ok = jsonDocument["ok"];
	if (!ok) {
		serialLog("getNewMessage error: ");
		serialLog(jsonDocument);
		serialLog("\n");
		return CTBotMessageNoData;
	}

	serialLog("getNewMessage JSON: ");
	serialLog(jsonDocument);
	serialLog("\n");

	uint32_t updateID = jsonDocument["result"][0]["update_id"].as<uint32_t>();
	if (updateID == 0)
		return CTBotMessageNoData;
	m_lastUpdate = updateID + 1;

	if (!jsonDocument["result"][0]["callback_query"]["id"].isNull()) {
		// this is a callback query
		serialLog("Callback query\n");
		message.messageID         = jsonDocument["result"][0]["callback_query"]["message"]["message_id"].as<int32_t>();
		message.text              = jsonDocument["result"][0]["callback_query"]["message"]["text"]		.as<String>();
		message.date              = jsonDocument["result"][0]["callback_query"]["message"]["date"]		.as<int32_t>();
		message.sender.id         = jsonDocument["result"][0]["callback_query"]["from"]["id"]			.as<int32_t>();
		message.sender.username   = jsonDocument["result"][0]["callback_query"]["from"]["username"]		.as<String>();
		message.sender.firstName  = jsonDocument["result"][0]["callback_query"]["from"]["first_name"]	.as<String>();
		message.sender.lastName   = jsonDocument["result"][0]["callback_query"]["from"]["last_name"]	.as<String>();
		message.callbackQueryID   = jsonDocument["result"][0]["callback_query"]["id"]					.as<String>();
		message.callbackQueryData = jsonDocument["result"][0]["callback_query"]["data"]					.as<String>();
		message.chatInstance      = jsonDocument["result"][0]["callback_query"]["chat_instance"]		.as<String>();
		message.messageType       = CTBotMessageQuery;
		return CTBotMessageQuery;
	}
	else if (!jsonDocument["result"][0]["message"]["message_id"].isNull()) {
		// this is a message
		message.messageID        = jsonDocument["result"][0]["message"]["message_id"]			.as<int32_t>();
		message.sender.id        = jsonDocument["result"][0]["message"]["from"]["id"]			.as<int32_t>();
		message.sender.username  = jsonDocument["result"][0]["message"]["from"]["username"]		.as<String>();
		message.sender.firstName = jsonDocument["result"][0]["message"]["from"]["first_name"]	.as<String>();
		message.sender.lastName  = jsonDocument["result"][0]["message"]["from"]["last_name"]	.as<String>();
		message.group.id         = jsonDocument["result"][0]["message"]["chat"]["id"]			.as<int64_t>();
		message.group.title      = jsonDocument["result"][0]["message"]["chat"]["title"]		.as<String>();
		message.date             = jsonDocument["result"][0]["message"]["date"]					.as<int32_t>();

		if (!jsonDocument["result"][0]["message"]["text"].isNull()) {
			// this is a text message
		    message.text        = jsonDocument["result"][0]["message"]["text"].as<String>();		    
			message.messageType = CTBotMessageText;
			return CTBotMessageText;
		}
		else if (!jsonDocument["result"][0]["message"]["location"].isNull()) {
			// this is a location message
			message.location.longitude = jsonDocument["result"][0]["message"]["location"]["longitude"]	.as<float>();
			message.location.latitude = jsonDocument["result"][0]["message"]["location"]["latitude"]	.as<float>();
			message.messageType = CTBotMessageLocation;
			return CTBotMessageLocation;
		}
		else if (!jsonDocument["result"][0]["message"]["contact"].isNull()) {
			// this is a contact message
			message.contact.id          = jsonDocument["result"][0]["message"]["contact"]["user_id"]		.as<int32_t>();
			message.contact.firstName   = jsonDocument["result"][0]["message"]["contact"]["first_name"]		.as<String>();
			message.contact.lastName    = jsonDocument["result"][0]["message"]["contact"]["last_name"]		.as<String>();
			message.contact.phoneNumber = jsonDocument["result"][0]["message"]["contact"]["phone_number"]	.as<String>();
			message.contact.vCard       = jsonDocument["result"][0]["message"]["contact"]["vcard"]			.as<String>();
			message.messageType = CTBotMessageContact;
			return CTBotMessageContact;
		}
	}
	// no valid/handled message
	return CTBotMessageNoData;
}

bool CTBot::sendMessage(int64_t id, String message, String keyboard)
{
	if (0 == message.length())
		return false;

	String strID = int64ToAscii(id);

	message = URLEncodeMessage(message); //-------------------------------------------------------------------------------------------------------------------------------------

	String parameters = (String)"?chat_id=" + strID + (String)"&text=" + message;

	if (keyboard.length() != 0)
		parameters += (String)"&reply_markup=" + keyboard;

	String response = sendCommand("sendMessage", parameters);
	if (response.length() == 0) {
		serialLog("SendMessage error: response with no data\n");
		return false;
	}

	DynamicJsonDocument jsonDocument(CTBOT_BUFFER_SIZE);
	
	if(!deserializeDoc(jsonDocument, response)) return CTBotMessageNoData;

	bool ok = jsonDocument["ok"];
	if (!ok) {
		serialLog("SendMessage error: ");
		serialLog(jsonDocument);
		serialLog("\n");
		return false;
	}

	serialLog("SendMessage JSON: ");
	serialLog(jsonDocument);
	serialLog("\n");

	return true;
}

bool CTBot::sendMessage(int64_t id, String message, CTBotInlineKeyboard &keyboard) {
	return sendMessage(id, message, keyboard.getJSON());
}

bool CTBot::sendMessage(int64_t id, String message, CTBotReplyKeyboard &keyboard) {
	return sendMessage(id, message, keyboard.getJSON());
}

bool CTBot::endQuery(String queryID, String message, bool alertMode)
{
	if (0 == queryID.length())
		return false;

	String parameters = (String)"?callback_query_id=" + queryID;

	if (message.length() != 0) {
		
		message = URLEncodeMessage(message); //---------------------------------------------------------------------------------------------------------------------------------

		if (alertMode)
			parameters += (String)"&text=" + message + (String)"&show_alert=true";
		else
			parameters += (String)"&text=" + message + (String)"&show_alert=false";
	}

	String response = sendCommand("answerCallbackQuery", parameters);
	if (response.length() == 0)
		return false;

	DynamicJsonDocument jsonDocument(CTBOT_BUFFER_SIZE);
	if(!deserializeDoc(jsonDocument, response)) return CTBotMessageNoData;

	bool ok = jsonDocument["ok"];
	if (!ok) {
		serialLog("answerCallbackQuery error:");
		serialLog(jsonDocument);
		serialLog("\n");
		return false;
	}

	serialLog(jsonDocument);
	serialLog("\n");
	return true;
}

bool CTBot::removeReplyKeyboard(int64_t id, String message, bool selective)
{
	StaticJsonDocument<JSON_OBJECT_SIZE(2)> jsonDocument;
	String command;
	JsonObject root = jsonDocument.to<JsonObject>();
	root["remove_keyboard"] = true;
	if (selective) {
		root["selective"] = true;
	}

	serializeJson(jsonDocument, command);
	return sendMessage(id, message, command);
}





// ----------------------------| STUBS - FOR BACKWARD VERSION COMPATIBILITY


bool CTBot::useDNS(bool value)
{	
	return(m_connection.useDNS(value));
}

void CTBot::setMaxConnectionRetries(uint8_t retries)
{	
	m_wifi.setMaxConnectionRetries(retries);
}

void CTBot::setStatusPin(int8_t pin)
{	
	m_connection.setStatusPin(pin);
}

void CTBot::setFingerprint(const uint8_t * newFingerprint)
{	
	m_connection.setFingerprint(newFingerprint);
}

bool CTBot::setIP(String ip, String gateway, String subnetMask, String dns1, String dns2) const 
{	
	return(m_wifi.setIP(ip, gateway, subnetMask, dns1, dns2));
}

bool CTBot::wifiConnect(String ssid, String password) const
{
	return(m_wifi.wifiConnect(ssid, password));
}

