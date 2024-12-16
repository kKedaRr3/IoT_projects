package org.example;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;
import org.eclipse.paho.client.mqttv3.IMqttMessageListener;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttMessage;

public class MqttMessageListener implements IMqttMessageListener {

    int responseId = 1;
    private final MqttClient client;
    private final ObjectMapper objectMapper = new ObjectMapper();

    MqttMessageListener(MqttClient client) {
        this.client = client;
        objectMapper.enable(SerializationFeature.INDENT_OUTPUT);
    }

    @Override
    public void messageArrived(String topic, MqttMessage message) throws Exception {
        System.out.println("Wiadomość na temacie: " + topic);
        String messageString = new String(message.getPayload());

        try {
            Object json = objectMapper.readValue(messageString, Object.class);
            String formattedJson = objectMapper.writerWithDefaultPrettyPrinter().writeValueAsString(json);
            System.out.println("Treść wiadomości: " + formattedJson);
        } catch (Exception e) {
            System.out.println("Błąd podczas formatowania JSON: " + e.getMessage());
        }

        if(topic.equals("/data/accelerometer")){
            String responseMessage = "Response_Id: " + responseId++;
            String responseTopic = "/response/accelerometer";
            client.publish(responseTopic, new MqttMessage(responseMessage.getBytes()));
            System.out.println("Response to data from accelerometer: " + responseMessage + "\n");
        }
        if(topic.equals("/data/magnetometer")){
            String responseMessage = "Response_Id: " + responseId++;
            String responseTopic = "/response/magnetometer";
            client.publish(responseTopic, new MqttMessage(responseMessage.getBytes()));
            System.out.println("Response to data from magnetometer: " + responseMessage + "\n");
        }

    }
}
