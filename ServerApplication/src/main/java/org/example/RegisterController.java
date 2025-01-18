package org.example;

import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.stage.Stage;
import javafx.event.ActionEvent;
import javafx.scene.control.PasswordField;
import javafx.scene.control.TextField;
import javafx.scene.control.Alert;
import javafx.scene.control.Alert.AlertType;

import java.io.IOException;

public class RegisterController {

    @FXML
    private TextField usernameField;

    @FXML
    private PasswordField passwordField;

    @FXML
    private PasswordField confirmPasswordField;

    // Obsługa rejestracji
    public void handleRegister() {
        String username = usernameField.getText();
        String password = passwordField.getText();
        String confirmPassword = confirmPasswordField.getText();

        // Sprawdzanie, czy nazwa użytkownika już istnieje
        if (UserDAO.usernameExists(username)) {
            showAlert("Username already exists", "The username you have entered is already taken. Please choose a different one.");
            return;  // Zakończ rejestrację
        }

        if (!password.equals(confirmPassword)) {
            System.out.println("Passwords do not match.");
            showAlert("Password mismatch", "The passwords you entered do not match. Please try again.");
        } else {
            System.out.println("Registration successful for user: " + username);
            UserDAO.addUser(username, password);  // Dodaj użytkownika do bazy
            showAlert("Registration successful", "Your registration was successful!");
        }
    }

    // Powrót do ekranu logowania
    @FXML
    public void goToLogin(ActionEvent event) throws IOException {
        // Pobranie bieżącego Stage
        Stage stage = (Stage) ((javafx.scene.Node) event.getSource()).getScene().getWindow();
        // Załadowanie widoku logowania
        Parent loginRoot = FXMLLoader.load(getClass().getResource("/login.fxml"));
        // Ustawienie nowej sceny
        stage.setScene(new Scene(loginRoot, 1600, 1200));
    }

    // Funkcja do wyświetlania komunikatów o błędach lub sukcesie
    private void showAlert(String title, String message) {
        Alert alert = new Alert(AlertType.INFORMATION);
        alert.setTitle(title);
        alert.setHeaderText(null);
        alert.setContentText(message);
        alert.showAndWait();
    }
}