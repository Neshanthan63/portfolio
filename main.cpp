#include "MQTTAsync.h"
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <map>
#include <atomic>
#include <sstream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

#define BROKER_ADDRESS "tcp://localhost:1883"
#define SENSOR_TOPIC "sensor/data"
#define ALERT_TOPIC "sensor/alerts"
#define CONTROL_TOPIC "control/commands"
#define FEEDBACK_TOPIC "sensor/feedback"
#define QOS 1
#define INTERVAL 5

std::atomic<bool> g_connected{ false };
std::atomic<bool> g_stop{ false };
std::atomic<bool> g_fan_on{ false };
std::atomic<bool> g_irrigation_on{ false };
std::ifstream sensorFile;

std::map<std::string, std::string> parseSensorLine(const std::string& line) {
    std::map<std::string, std::string> sensorData;
    size_t pos = 0, next = 0;

    while ((next = line.find(',', pos)) != std::string::npos) {
        size_t colon = line.find(':', pos);
        if (colon != std::string::npos && colon < next) {
            std::string key = line.substr(pos, colon - pos);
            std::string value = line.substr(colon + 1, next - colon - 1);

            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            sensorData[key] = value;
        }
        pos = next + 1;
    }
    return sensorData;
}

void checkAndPublishAlerts(MQTTAsync client, const std::map<std::string, std::string>& sensorData) {
    std::string alertMessage;
    int temperatureThreshold = 50;
    int pestLevelThreshold = 75;
    int humidityLowThreshold = 20;

    auto tempIt = sensorData.find("Temperature");
    if (tempIt != sensorData.end() && !tempIt->second.empty() && std::stoi(tempIt->second) > temperatureThreshold) {
        alertMessage += "Temperature exceeded threshold. ";
    }

    auto pestIt = sensorData.find("Pest Level");
    if (pestIt != sensorData.end() && !pestIt->second.empty() && std::stoi(pestIt->second) > pestLevelThreshold) {
        alertMessage += "Pest Level is critical. ";
    }

    auto humidityIt = sensorData.find("Humidity Level");
    if (humidityIt != sensorData.end() && !humidityIt->second.empty() && std::stoi(humidityIt->second) < humidityLowThreshold) {
        alertMessage += "Humidity is too low. ";
    }

    if (!alertMessage.empty()) {
        MQTTAsync_message alert = MQTTAsync_message_initializer;
        alert.payload = (void*)alertMessage.c_str();
        alert.payloadlen = static_cast<int>(alertMessage.size());
        alert.qos = QOS;
        alert.retained = 0;

        int rc = MQTTAsync_sendMessage(client, ALERT_TOPIC, &alert, NULL);
        if (rc != MQTTASYNC_SUCCESS) {
            std::cerr << "Failed to send alert message. Error code: " << rc << std::endl;
        }
        else {
            std::cout << "Alert sent: " << alertMessage << std::endl;
        }
    }
}

void publishFeedback(MQTTAsync client, const std::string& message) {
    MQTTAsync_message fb_message = MQTTAsync_message_initializer;
    fb_message.payload = (void*)message.c_str();
    fb_message.payloadlen = static_cast<int>(message.size());
    fb_message.qos = QOS;
    fb_message.retained = 0;

    int rc = MQTTAsync_sendMessage(client, FEEDBACK_TOPIC, &fb_message, NULL);
    if (rc != MQTTASYNC_SUCCESS) {
        std::cerr << "Failed to send feedback. Error code: " << rc << std::endl;
    }
}

int onMessageReceived(void* context, char* topicName, int topicLen, MQTTAsync_message* message) {
    std::string controlMessage(static_cast<char*>(message->payload), message->payloadlen);
    std::cout << "Control command received on topic '" << topicName << "': " << controlMessage << std::endl;

    if (std::string(topicName) == CONTROL_TOPIC) {
        try {
            json jsonCommand = json::parse(controlMessage);
            std::string command = jsonCommand.value("command", "");

            if (command == "STOP") {
                g_stop = true;
                std::cout << "Stopping sensor data transmission." << std::endl;
                publishFeedback(static_cast<MQTTAsync>(context), "Processed command: STOP");
            }
            else if (command == "FAN") {
                std::string action = jsonCommand.value("action", "");
                if (action == "ON") {
                    g_fan_on = true;
                    std::cout << "Turning FAN ON." << std::endl;
                    publishFeedback(static_cast<MQTTAsync>(context), "Processed command: FAN ON");
                }
                else if (action == "OFF") {
                    g_fan_on = false;
                    std::cout << "Turning FAN OFF." << std::endl;
                    publishFeedback(static_cast<MQTTAsync>(context), "Processed command: FAN OFF");
                }
                else {
                    std::cerr << "Invalid action for FAN command." << std::endl;
                    publishFeedback(static_cast<MQTTAsync>(context), "Error: Invalid action for FAN");
                }
            }
            else if (command == "IRRIGATION") {
                std::string action = jsonCommand.value("action", "");
                if (action == "ON") {
                    g_irrigation_on = true;
                    std::cout << "Turning IRRIGATION ON." << std::endl;
                    publishFeedback(static_cast<MQTTAsync>(context), "Processed command: IRRIGATION ON");
                }
                else if (action == "OFF") {
                    g_irrigation_on = false;
                    std::cout << "Turning IRRIGATION OFF." << std::endl;
                    publishFeedback(static_cast<MQTTAsync>(context), "Processed command: IRRIGATION OFF");
                }
                else {
                    std::cerr << "Invalid action for IRRIGATION command." << std::endl;
                    publishFeedback(static_cast<MQTTAsync>(context), "Error: Invalid action for IRRIGATION");
                }
            }
            else {
                std::cout << "Unknown control command: " << command << std::endl;
                publishFeedback(static_cast<MQTTAsync>(context), "Unknown command: " + command);
            }
        }
        catch (const json::parse_error& e) {
            std::cerr << "Error parsing control command JSON: " << e.what() << std::endl;
            publishFeedback(static_cast<MQTTAsync>(context), "Error parsing command JSON");
        }
    }

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void onConnectionLost(void* context, char* cause) {
    std::cerr << "Connection lost: " << (cause ? cause : "Unknown") << std::endl;
    g_connected = false;
}

void onConnectionSuccess(void* context, MQTTAsync_successData* response) {
    std::cout << "Successfully connected to the broker!" << std::endl;
    g_connected = true;

    MQTTAsync client = static_cast<MQTTAsync>(context);
    int rc = MQTTAsync_subscribe(client, CONTROL_TOPIC, QOS, NULL);
    if (rc != MQTTASYNC_SUCCESS) {
        std::cerr << "Failed to subscribe to control topic. Error code: " << rc << std::endl;
    }
    else {
        std::cout << "Subscribed to control topic: " << CONTROL_TOPIC << std::endl;
    }
}

void onConnectionFailure(void* context, MQTTAsync_failureData* response) {
    std::cerr << "Failed to connect to the broker." << std::endl;
    if (response && response->message) {
        std::cerr << "Error message: " << response->message << std::endl;
    }
    g_connected = false;
}

int main() {
    MQTTAsync client;
    int rc;

    std::string clientId = "SensorClient_" + std::to_string(time(NULL));
    rc = MQTTAsync_create(&client, BROKER_ADDRESS, clientId.c_str(), MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTASYNC_SUCCESS) {
        std::cerr << "Failed to create MQTT client. Error code: " << rc << std::endl;
        return -1;
    }

    rc = MQTTAsync_setCallbacks(client, client, onConnectionLost, onMessageReceived, NULL);
    if (rc != MQTTASYNC_SUCCESS) {
        std::cerr << "Failed to set callbacks. Error code: " << rc << std::endl;
        MQTTAsync_destroy(&client);
        return -1;
    }

    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    conn_opts.onSuccess = onConnectionSuccess;
    conn_opts.onFailure = onConnectionFailure;
    conn_opts.context = client;

    rc = MQTTAsync_connect(client, &conn_opts);
    if (rc != MQTTASYNC_SUCCESS) {
        std::cerr << "Failed to connect to the broker. Error code: " << rc << std::endl;
        MQTTAsync_destroy(&client);
        return -1;
    }

    while (!g_connected) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Starting sensor data transmission..." << std::endl;

    sensorFile.open("sensor_data.txt");
    if (!sensorFile.is_open()) {
        std::cerr << "Failed to open sensor.txt file. Ensure it exists in the same directory." << std::endl;
        return -1;
    }

    while (!g_stop) {
        std::string line;
        if (std::getline(sensorFile, line)) {
            if (line.empty()) continue;

            auto sensorData = parseSensorLine(line);

            json jsonData;
            for (const auto& pair : sensorData) {
                jsonData[pair.first] = pair.second;
            }
            jsonData["FAN"] = g_fan_on ? "ON" : "OFF";
            jsonData["IRRIGATION"] = g_irrigation_on ? "ON" : "OFF";
            std::string jsonPayload = jsonData.dump();

            MQTTAsync_message message = MQTTAsync_message_initializer;
            message.payload = (void*)jsonPayload.c_str();
            message.payloadlen = static_cast<int>(jsonPayload.size());
            message.qos = QOS;
            message.retained = 0;

            rc = MQTTAsync_sendMessage(client, SENSOR_TOPIC, &message, NULL);
            if (rc != MQTTASYNC_SUCCESS) {
                std::cerr << "Failed to send sensor data. Error code: " << rc << std::endl;
            }
            else {
                std::cout << "Published to " << SENSOR_TOPIC << ": " << jsonPayload << std::endl;
                checkAndPublishAlerts(client, sensorData);
            }
        }
        else {
            sensorFile.clear();
            sensorFile.seekg(0, std::ios::beg);
            std::cout << "Reached end of sensor.txt, looping back to the start." << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(INTERVAL));
    }

    sensorFile.close();
    MQTTAsync_disconnect(client, NULL);
    MQTTAsync_destroy(&client);
    std::cout << "Disconnected and client destroyed." << std::endl;
    return 0;
}