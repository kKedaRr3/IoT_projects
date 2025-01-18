package org.example;

import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.PasswordField;
import javafx.scene.control.TextField;
import javafx.scene.control.Label;
import javafx.stage.Stage;

import java.io.IOException;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

public class LoginController {

    @FXML
    private TextField usernameField; // Musi odpowiadać fx:id z FXML
    @FXML
    private PasswordField passwordField; // Musi odpowiadać fx:id z FXML
    @FXML
    private Label errorLabel; // Musi odpowiadać fx:id z FXML

    public static String loggedInUser = null;

    public void handleLogin() {
        String username = usernameField.getText(); // Pobranie tekstu z pola
        String password = passwordField.getText();  // Pobranie hasła z pola

        // Sprawdzenie, czy użytkownik o podanej nazwie istnieje w bazie
        try (Connection conn = DatabaseConnection.connect()) {
            if (conn != null) {
                String query = "SELECT password FROM users WHERE username = ?";
                try (PreparedStatement stmt = conn.prepareStatement(query)) {
                    stmt.setString(1, username); // Ustawienie nazwy użytkownika w zapytaniu

                    ResultSet rs = stmt.executeQuery();

                    if (rs.next()) {
                        // Pobierz zapisane hasło (możesz je później porównać z wprowadzonym)
                        String storedPasswordHash = rs.getString("password");

                        // Porównaj hasła (tutaj powinieneś używać metody hashującej, np. bcrypt)
                        if (storedPasswordHash.equals(password)) {  // Proste porównanie haseł
                            errorLabel.setText("Login successful!");
                            loggedInUser = username;
                            try{
                                Stage stage = (Stage) usernameField.getScene().getWindow();
                                Parent dashboardRoot = FXMLLoader.load(getClass().getResource("/dashboard.fxml"));
                                stage.setScene(new Scene(dashboardRoot, 1600, 1200));  // Ustaw nową scenę
                            }
                            catch (Exception e){
                                e.printStackTrace();
                            }
                        } else {
                            errorLabel.setText("Invalid username or password.");
                        }
                    } else {
                        errorLabel.setText("User not found.");
                    }
                }
            }
        } catch (SQLException e) {
            e.printStackTrace();
            errorLabel.setText("Database error.");
        }
    }

    @FXML
    public void goToRegister(javafx.event.ActionEvent event) throws IOException {
        // Pobierz Stage z eventu
        Stage stage = (Stage) ((javafx.scene.Node) event.getSource()).getScene().getWindow();
        // Załaduj widok rejestracji
        Parent registerRoot = FXMLLoader.load(getClass().getResource("/register.fxml"));
        // Ustaw scenę
        stage.setScene(new Scene(registerRoot, 1600, 1200));
    }
}
