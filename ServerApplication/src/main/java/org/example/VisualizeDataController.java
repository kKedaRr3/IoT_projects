package org.example;

import javafx.application.Platform;
import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.scene.Scene;
import javafx.scene.chart.LineChart;
import javafx.scene.chart.NumberAxis;
import javafx.scene.chart.XYChart;
import javafx.scene.layout.AnchorPane;
import javafx.stage.Stage;

import java.io.IOException;
import java.util.LinkedList;
import java.util.Queue;

public class VisualizeDataController {

    @FXML
    private LineChart<Number, Number> accelerationChart;  // Wykres przyspieszenia

    private XYChart.Series<Number, Number> accelerometerSeries; // Dane wykresu

    // Bufer do przechowywania danych
    private Queue<String> dataBuffer = new LinkedList<>();

    @FXML
    public void initialize() {
        // Inicjalizacja serii danych
        accelerometerSeries = new XYChart.Series<>();
        accelerometerSeries.setName("Acceleration Data");

        // Dodaj serię do wykresu
        accelerationChart.getData().add(accelerometerSeries);

        // Rozpocznij subskrypcję danych z tematu
        startDataSubscription();
    }

    private void startDataSubscription() {
        Thread dataVisualizationThread = new Thread(() -> {
            while (true) {
                try {
                    Thread.sleep(100); // Odświeżanie co 100 ms

                    // Sprawdź, czy są nowe dane na temacie "accelerometer"
                    if (!MqttController.accelerometerMessages.isEmpty()) {
                        String data = MqttController.accelerometerMessages.remove(0);

                        // Zapisz dane do bufora
                        dataBuffer.add(data);
                    }

                    // Jeśli wykres jest widoczny, zaktualizuj go
                    if (accelerationChart.isVisible() && !dataBuffer.isEmpty()) {
                        String data = dataBuffer.poll();  // Pobierz dane z bufora

                        // Zaktualizuj wykres
                        Platform.runLater(() -> updateChart(data));
                    }

                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        });

        dataVisualizationThread.setDaemon(true);
        dataVisualizationThread.start();
    }

    private void updateChart(String data) {
        try {
            // Dane w formacie: x,y,z,timestamp
            String[] components = data.split(",");
            if (components.length == 4) {
                double x = Double.parseDouble(components[0]);
                double y = Double.parseDouble(components[1]);
                double z = Double.parseDouble(components[2]);
                long timestamp = Long.parseLong(components[3]);

                // Oblicz wartość przyspieszenia
                double acceleration = Math.sqrt(x * x + y * y + z * z);

                // Dodaj punkt do wykresu
                accelerometerSeries.getData().add(new XYChart.Data<>(timestamp, acceleration));

                // Ogranicz liczbę punktów na wykresie, aby uniknąć przepełnienia
                if (accelerometerSeries.getData().size() > 1000) {
                    accelerometerSeries.getData().remove(0); // Usuwa najstarszy punkt
                }

                // Zaktualizuj zakres osi X, aby wykres przesuwał się
                adjustXAxis();
            }
        } catch (Exception e) {
            System.err.println("Błąd przetwarzania danych: " + data);
            e.printStackTrace();
        }
    }

    private void adjustXAxis() {
        // Uzyskaj zakres wartości z danych
        double minX = Double.MAX_VALUE;
        double maxX = Double.MIN_VALUE;

        for (XYChart.Data<Number, Number> dataPoint : accelerometerSeries.getData()) {
            double timestamp = dataPoint.getXValue().doubleValue();
            if (timestamp < minX) minX = timestamp;
            if (timestamp > maxX) maxX = timestamp;
        }

        // Uzyskaj dostęp do osi X jako NumberAxis
        NumberAxis xAxis = (NumberAxis) accelerationChart.getXAxis();

        // Zaktualizuj zakres osi X na podstawie danych
        xAxis.setLowerBound(minX);  // Ustawienie dolnej granicy
        xAxis.setUpperBound(maxX);  // Ustawienie górnej granicy
    }

    @FXML
    public void handleBackToDashboard() {
        try {
            // Załaduj plik FXML Dashboard
            FXMLLoader loader = new FXMLLoader(getClass().getResource("/dashboard.fxml"));
            AnchorPane dashboard = loader.load();

            // Uzyskaj dostęp do okna (Stage)
            Stage stage = (Stage) accelerationChart.getScene().getWindow(); // Zamiast espMacField
            Scene scene = new Scene(dashboard, 1600, 1200);
            stage.setScene(scene);  // Ustaw nową scenę na oknie
            stage.show();  // Pokaż okno
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
