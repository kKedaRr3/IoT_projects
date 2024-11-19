package org.example;

import org.eclipse.paho.client.mqttv3.IMqttMessageListener;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttMessage;

public class MqttMessageListener implements IMqttMessageListener {

    int responseId = 1;
    private MqttClient client;

    MqttMessageListener(MqttClient client) {
        this.client = client;
    }

    @Override
    public void messageArrived(String topic, MqttMessage message) throws Exception {
        System.out.println("Wiadomość na temacie: " + topic);
        System.out.println("Treść wiadomości: " + new String(message.getPayload()));


        if(topic.equals("/topic/data1")){
            String responseMessage = "Response_Id: " + responseId++;
            String responseTopic = "/topic/response1";
            client.publish(responseTopic, new MqttMessage(responseMessage.getBytes()));
            System.out.println("Response to data" + "1: " + responseMessage + "\n");
        }
        if(topic.equals("/topic/data2")){
            String responseMessage = "Response_Id: " + responseId++;
            String responseTopic = "/topic/response2";
            client.publish(responseTopic, new MqttMessage(responseMessage.getBytes()));
            System.out.println("Response to data" + "2: " +responseMessage + "\n");
        }

    }
}
