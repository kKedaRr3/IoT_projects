package org.example;

import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.scene.Scene;
import javafx.scene.control.Label;
import javafx.scene.control.TextField;
import javafx.scene.layout.AnchorPane;
import javafx.stage.Stage;

import java.io.IOException;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.SQLException;

public class Esp32ConfigController {

    @FXML
    private TextField espMacField;
    @FXML
    private TextField espMFrequencyField;
    @FXML
    private TextField espExitThresholdField;
    @FXML
    private Label errorLabel;

    // Funkcja zapisująca ESP32 MAC Address, częstotliwość i próg wyjścia do bazy danych
    public void handleSaveEspConfig() {
        String espMac = espMacField.getText();
        String espMFrequency = espMFrequencyField.getText();
        String espExitThreshold = espExitThresholdField.getText();

        if (espMac != null && !espMac.isEmpty() &&
                espMFrequency != null && !espMFrequency.isEmpty() &&
                espExitThreshold != null && !espExitThreshold.isEmpty()) {

            try {
                // Konwertowanie wartości z tekstu do odpowiednich typów
                int frequency = Integer.parseInt(espMFrequency);
                double exitThreshold = Double.parseDouble(espExitThreshold);

                // Połączenie z bazą danych
                try (Connection conn = DatabaseConnection.connect()) {
                    String updateSql = "UPDATE users SET esp_mac = ?, esp_m_frequency = ?, esp_exit_threshold = ? WHERE username = ?";
                    try (PreparedStatement pstmt = conn.prepareStatement(updateSql)) {
                        pstmt.setString(1, espMac);  // Ustawienie MAC address
                        pstmt.setInt(2, frequency);  // Ustawienie częstotliwości
                        pstmt.setDouble(3, exitThreshold);  // Ustawienie progu wyjścia
                        pstmt.setString(4, LoginController.loggedInUser);  // Użycie zalogowanego użytkownika
                        pstmt.executeUpdate();
                        System.out.println("ESP32 configuration saved!");
                        errorLabel.setText("ESP32 configuration saved successfully!");
                    }
                }
            } catch (SQLException e) {
                // Obsługa błędów związanych z SQL
                System.out.println("SQL Error: " + e.getMessage());
                errorLabel.setText("An error occurred while saving the configuration.");
            } catch (NumberFormatException e) {
                // Obsługa błędów przy parsowaniu częstotliwości i progu
                errorLabel.setText("Invalid number format for frequency or exit threshold.");
            }
        } else {
            errorLabel.setText("Error: All fields must be filled in.");
        }
    }

    // Funkcja do czyszczenia MAC Address (ustawienie NULL)
    public void handleClearEspMac() {
        try (Connection conn = DatabaseConnection.connect()) {
            String updateSql = "UPDATE users SET esp_mac = NULL WHERE username = ?";
            try (PreparedStatement pstmt = conn.prepareStatement(updateSql)) {
                pstmt.setString(1, LoginController.loggedInUser);  // Użycie zalogowanego użytkownika
                pstmt.executeUpdate();
                System.out.println("ESP32 MAC Address cleared!");
            }
        } catch (SQLException e) {
            e.printStackTrace();
        }
    }

    public void handleBackToDashboard() {
        try {
            // Załaduj plik FXML Dashboard
            FXMLLoader loader = new FXMLLoader(getClass().getResource("/dashboard.fxml"));
            AnchorPane dashboard = loader.load();

            // Uzyskaj dostęp do okna (Stage)
            Stage stage = (Stage) espMacField.getScene().getWindow();
            Scene scene = new Scene(dashboard, 1600, 1200);
            stage.setScene(scene);  // Ustaw nową scenę na oknie
            stage.show();  // Pokaż okno
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
