package org.example;

import javafx.application.Application;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.stage.Stage;

public class Main extends Application {

    private Stage primaryStage;

    @Override
    public void start(Stage primaryStage) throws Exception {
        this.primaryStage = primaryStage;

        // Załaduj ekran logowania po uruchomieniu aplikacji
        showLoginView();
    }

    private void showLoginView() throws Exception {
        Parent loginRoot = FXMLLoader.load(getClass().getResource("/login.fxml"));
        primaryStage.setTitle("Login");
        primaryStage.setScene(new Scene(loginRoot, 1600, 1200));
        primaryStage.show();
    }

    public static void main(String[] args) {
        launch(args);
    }
}
