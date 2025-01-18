package org.example;

import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.Label;
import javafx.scene.control.TextArea;
import javafx.stage.Stage;

public class DashboardController {

    @FXML
    private Label welcomeLabel;

    @FXML
    public void handleLogOut() {
        // Logika wylogowywania, np. przejście do ekranu logowania
        try {
            // Wylogowanie użytkownika
            LoginController.loggedInUser = null;
            DataStorage.reset();
            DataStorage.setFirstTimestamp(-1);

            // Jeśli jesteśmy połączeni, rozłączamy się
            if (MqttService.isConnected()) {
                MqttService.disconnect();  // Używamy MqttService do rozłączenia
            }

            // Aktualizacja statusu połączenia
            MqttService.setIsConnected(false);

            // Zmiana widoku na ekran logowania
            Stage stage = (Stage) welcomeLabel.getScene().getWindow();
            Parent loginRoot = FXMLLoader.load(getClass().getResource("/login.fxml"));
            stage.setScene(new Scene(loginRoot, 1600, 1200));
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @FXML
    public void handleEsp32Config() {
        try {

            Stage stage = (Stage) welcomeLabel.getScene().getWindow();
            // Załaduj widok ESP32 Config
            FXMLLoader loader = new FXMLLoader(getClass().getResource("/esp32_config.fxml"));
            Parent esp32ConfigRoot = loader.load();
            stage.setScene(new Scene(esp32ConfigRoot, 1600, 1200));  // Zmiana sceny na ESP32 Config
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @FXML
    public void handleMqttView() {
        try {
            Stage stage = (Stage) welcomeLabel.getScene().getWindow();
            FXMLLoader loader = new FXMLLoader(getClass().getResource("/mqtt_view.fxml"));
            Parent mqttViewRoot = loader.load();
            stage.setScene(new Scene(mqttViewRoot, 1600, 1200));
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @FXML
    public void handleVisualizeData() {
        try {
            Stage stage = (Stage) welcomeLabel.getScene().getWindow();
            // Załaduj widok do wizualizacji danych 3D
            FXMLLoader loader = new FXMLLoader(getClass().getResource("/visualize_data.fxml"));
            Parent visualizeDataRoot = loader.load();
            stage.setScene(new Scene(visualizeDataRoot, 1600, 1200));
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

}
