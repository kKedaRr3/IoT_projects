package org.example;

import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;

public class MqttService {
    private static MqttClient mqttClient;
    private static boolean isConnected = false;

    public static MqttClient getMqttClient() {
        return mqttClient;
    }

    public static boolean isConnected() {
        return isConnected;
    }

    public static void setIsConnected(boolean isConnected) {
        MqttService.isConnected = isConnected;
    }

    public static void connect(String brokerAddress) throws MqttException {
        if (mqttClient == null || !isConnected) {
            mqttClient = new MqttClient(brokerAddress, MqttClient.generateClientId());
            MqttConnectOptions options = new MqttConnectOptions();
            options.setCleanSession(true);
            mqttClient.connect(options);
            isConnected = true;
            System.out.println("Connected to broker: " + brokerAddress);
        }
    }

    public static void disconnect() throws MqttException {
        if (mqttClient != null && mqttClient.isConnected()) {
            mqttClient.disconnect();
            isConnected = false;
            System.out.println("Disconnected from broker.");
        }
    }
}

