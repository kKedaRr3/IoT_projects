package org.example;

import java.sql.*;

public class UserDAO {

    // Funkcja dodająca użytkownika
    public static void addUser(String username, String password) {
        String sql = "INSERT INTO users(username, password) VALUES(?, ?)";

        createTable();

        try (Connection conn = DatabaseConnection.connect();
             PreparedStatement pstmt = conn.prepareStatement(sql)) {

            pstmt.setString(1, username);
            pstmt.setString(2, password);
            pstmt.executeUpdate();

            System.out.println("Użytkownik dodany!");
        } catch (SQLException e) {
            System.out.println("Błąd podczas dodawania użytkownika: " + e.getMessage());
        }
    }

    // Funkcja tworząca tabelę użytkowników, jeśli jeszcze nie istnieje
    private static void createTable() {
        String createTableSQL = """
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT NOT NULL,
                password TEXT NOT NULL,
                esp_mac TEXT UNIQUE,
                esp_m_frequency INTEGER,
                esp_exit_threshold DOUBLE
            );
        """;

        try (Connection conn = DatabaseConnection.connect();
             Statement stmt = conn.createStatement()) {

            if (conn != null) {
                stmt.execute(createTableSQL);
                System.out.println("Tabela 'users' została utworzona lub już istnieje.");
            }

        } catch (SQLException e) {
            System.out.println("Wystąpił błąd podczas łączenia z bazą danych: " + e.getMessage());
        }
    }

    // Funkcja sprawdzająca, czy nazwa użytkownika już istnieje w bazie
    public static boolean usernameExists(String username) {
        String sql = "SELECT COUNT(*) FROM users WHERE username = ?";

        try (Connection conn = DatabaseConnection.connect();
             PreparedStatement pstmt = conn.prepareStatement(sql)) {

            pstmt.setString(1, username);
            ResultSet rs = pstmt.executeQuery();

            if (rs.next()) {
                return rs.getInt(1) > 0;  // Jeśli liczba rekordów > 0, użytkownik istnieje
            }

        } catch (SQLException e) {
            System.out.println("Błąd podczas sprawdzania użytkownika: " + e.getMessage());
        }

        return false;  // Jeśli nie znaleziono użytkownika, zwróć false
    }

    // Funkcja do pobrania MAC adresu ESP dla użytkownika
    public static String getEspMacForUser(String username) {
        String espMac = null;
        String sql = "SELECT esp_mac FROM users WHERE username = ?";

        try (Connection conn = DatabaseConnection.connect();
             PreparedStatement pstmt = conn.prepareStatement(sql)) {

            pstmt.setString(1, username);
            ResultSet rs = pstmt.executeQuery();

            if (rs.next()) {
                espMac = rs.getString("esp_mac");
            }

        } catch (SQLException e) {
            System.out.println("Błąd podczas pobierania ESP MAC: " + e.getMessage());
        }

        return espMac;
    }

    public static String getEspMFrequencyForUser(String user) {
        String frequency = null;
        String query = "SELECT esp_m_frequency FROM users WHERE username = ?";

        try (Connection conn = DatabaseConnection.connect();
             PreparedStatement pstmt = conn.prepareStatement(query)) {

            pstmt.setString(1, user);
            ResultSet rs = pstmt.executeQuery();

            if (rs.next()) {
                frequency = rs.getString("esp_m_frequency");
            }
        } catch (SQLException e) {
            System.out.println("Błąd podczas pobierania esp_m_frequency: " + e.getMessage());
        }

        return frequency;
    }

    // Metoda pobierająca wartość esp_exit_threshold z bazy danych dla danego esp_mac
    public static String getEspExitThresholdForUser(String user) {
        String threshold = null;
        String query = "SELECT esp_exit_threshold FROM users WHERE username = ?";

        try (Connection conn = DatabaseConnection.connect();
             PreparedStatement pstmt = conn.prepareStatement(query)) {

            pstmt.setString(1, user);
            ResultSet rs = pstmt.executeQuery();

            if (rs.next()) {
                threshold = rs.getString("esp_exit_threshold");
            }
        } catch (SQLException e) {
            System.out.println("Błąd podczas pobierania esp_exit_threshold: " + e.getMessage());
        }

        return threshold;
    }

}
