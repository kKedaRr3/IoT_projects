package org.example;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;

public class DatabaseConnection {
    public static Connection connect() {
        String url = "jdbc:sqlite:users.db"; // Ścieżka do pliku bazy danych
        try {
            Connection conn = DriverManager.getConnection(url);
            return conn;
        } catch (SQLException e) {
            System.out.println("Błąd połączenia z SQLite: " + e.getMessage());
            return null;
        }
    }
}
