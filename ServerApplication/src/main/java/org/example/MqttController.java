package org.example;

import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.scene.Scene;
import javafx.scene.control.Label;
import javafx.scene.control.TextArea;
import javafx.scene.control.TextField;
import javafx.scene.layout.AnchorPane;
import javafx.stage.Stage;
import org.eclipse.paho.client.mqttv3.*;

import java.io.IOException;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

public class MqttController {

    @FXML
    private TextField brokerAddressField;

    @FXML
    private Label connectionStatusLabel;

    @FXML
    private TextArea accMessagesArea;

    @FXML
    private Label espMacErrorLabel;

    public void initialize() {
        // Ustawienie domyślnej wartości w polu tekstowym
        brokerAddressField.setText("tcp://192.168.74.28");
        updateConnectionStatus(); // Aktualizuj status połączenia

        loadStoredMessages();
    }

    private void loadStoredMessages() {
        // Pobierz wszystkie istniejące wiadomości z DataStorage
        List<String> allMessages = DataStorage.getAllData(); // Zakładając, że masz metodę do pobierania danych

        // Wyczyść TextArea przed dodaniem nowych danych
        accMessagesArea.clear();

        // Dodaj każdą wiadomość do TextArea
        for (String message : allMessages) {
            accMessagesArea.appendText(message + "\n");
        }
    }

    // Metoda aktualizująca status połączenia
    private void updateConnectionStatus() {
        if (MqttService.isConnected()) {
            connectionStatusLabel.setText("Connected");
            connectionStatusLabel.setStyle("-fx-text-fill: green;");
        } else {
            connectionStatusLabel.setText("Disconnected");
            connectionStatusLabel.setStyle("-fx-text-fill: red;");
        }
    }

    // Metoda obsługująca łączenie
    public void handleConnect() {
        String brokerAddress = brokerAddressField.getText();

        if (brokerAddress == null || brokerAddress.isEmpty()) {
            connectionStatusLabel.setText("Broker address is required.");
            connectionStatusLabel.setStyle("-fx-text-fill: red;");
            return;
        }

        try {
            if (!brokerAddress.startsWith("tcp://") && !brokerAddress.startsWith("ssl://")) {
                connectionStatusLabel.setText("Invalid broker address. It must start with tcp:// or ssl://.");
                connectionStatusLabel.setStyle("-fx-text-fill: red;");
                return;
            }

            // Łączenie za pomocą MqttService
            MqttService.connect(brokerAddress);
            updateConnectionStatus();  // Aktualizuj status połączenia
            subscribeToTopics();  // Subskrybuj tematy po pomyślnym połączeniu

        } catch (MqttException e) {
            connectionStatusLabel.setText("Failed to connect to the broker.");
            connectionStatusLabel.setStyle("-fx-text-fill: red;");
            e.printStackTrace();
        } catch (Exception e) {
            connectionStatusLabel.setText("An unexpected error occurred.");
            connectionStatusLabel.setStyle("-fx-text-fill: red;");
            e.printStackTrace();
        }
    }

    // Metoda obsługująca rozłączanie
    public void handleDisconnect() {
        try {
            // Rozłączanie za pomocą MqttService
            MqttService.disconnect();
            updateConnectionStatus();  // Aktualizuj status po rozłączeniu
        } catch (MqttException e) {
            e.printStackTrace();
        }
    }

    // Subskrypcja tematów po udanym połączeniu
    private void subscribeToTopics() {
        try {
            // Pobierz adres MAC użytkownika (zakładając metodę z UserDAO)
            String espMac = UserDAO.getEspMacForUser(LoginController.loggedInUser);

            if (espMac == null || espMac.isEmpty()) {
                // Jeśli adres MAC jest pusty lub null, wyświetl błąd
                System.err.println("ESP MAC address is missing for the logged-in user.");
                espMacErrorLabel.setText("ESP MAC address is missing for the logged-in user.");
                espMacErrorLabel.setStyle("-fx-text-fill: red;");
                return;
            } else {
                // Jeśli adres MAC jest dostępny, wyczyść komunikat o błędzie
                espMacErrorLabel.setText("");
            }

            // Dodaj adres MAC jako prefiks do tematów
            String topic1 = "/" + espMac + "/data/accelerometer";

            // Subskrybuj temat
            MqttService.getMqttClient().subscribe(topic1, (topic, message) -> {
                String msg = new String(message.getPayload());
                accMessagesArea.appendText(msg +  "\n");
                DataStorage.addData(msg);
                System.out.println(msg);
            });

        } catch (MqttException e) {
            e.printStackTrace();
        } catch (Exception e) {
            System.err.println("Error while subscribing to topics: " + e.getMessage());
        }
    }

    // Metoda wysyłająca konfigurację do ESP
    public void handleSendConfig() {
        String espMac = UserDAO.getEspMacForUser(LoginController.loggedInUser);

        if (espMac == null || espMac.isEmpty()) {
            espMacErrorLabel.setText("ESP MAC address is missing for the logged-in user.");
            espMacErrorLabel.setStyle("-fx-text-fill: red;");
            return;
        }

        // Pobierz dane z bazy danych dla `esp_m_frequency` i `esp_exit_threshold`
        String espMFrequency = UserDAO.getEspMFrequencyForUser(LoginController.loggedInUser);

        if (espMFrequency != null) {
            String message = "esp_m_frequency=" + espMFrequency;
            String topic = "/" + espMac + "/data/config";

            try {
                MqttService.getMqttClient().publish(topic, new MqttMessage(message.getBytes()));
                System.out.println("Message sent to topic: " + topic);
            } catch (MqttException e) {
                e.printStackTrace();
                espMacErrorLabel.setText("Failed to send message.");
                espMacErrorLabel.setStyle("-fx-text-fill: red;");
            }
        }
    }


    // Metoda przechodząca do ekranu Dashboard
    public void handleBackToDashboard(javafx.event.ActionEvent event) {
        try {
            // Załaduj plik FXML Dashboard
            FXMLLoader loader = new FXMLLoader(getClass().getResource("/dashboard.fxml"));
            AnchorPane dashboard = loader.load();

            // Uzyskaj dostęp do okna (Stage)
            Stage stage = (Stage) ((javafx.scene.Node) event.getSource()).getScene().getWindow();
            Scene scene = new Scene(dashboard, 1600, 1200);
            stage.setScene(scene);  // Ustaw nową scenę na oknie
            stage.show();  // Pokaż okno
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
