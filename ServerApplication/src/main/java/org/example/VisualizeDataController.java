package org.example;

import javafx.application.Platform;
import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.scene.Scene;
import javafx.scene.chart.LineChart;
import javafx.scene.chart.NumberAxis;
import javafx.scene.chart.XYChart;
import javafx.scene.control.TextArea;
import javafx.scene.layout.AnchorPane;
import javafx.stage.Stage;

import java.io.IOException;
import java.util.List;

public class VisualizeDataController {

    private static final int MAX_VISIBLE_POINTS = 30; // Maksymalna liczba widocznych punktów na wykresie
    private static final long DATA_UPDATE_INTERVAL = 200; // Czas odświeżania danych w ms (200ms)

    @FXML
    private TextArea gapDetectionOutput;

    @FXML
    private LineChart<Number, Number> accelerationChart;

    private XYChart.Series<Number, Number> accelerometerSeries;

    @FXML
    public void initialize() {
        accelerometerSeries = new XYChart.Series<>();
        accelerometerSeries.setName("Acceleration Data");
        accelerationChart.getData().add(accelerometerSeries);

        // Przywracanie zapisanych danych (jeśli istnieją)
        List<XYChart.Data<Number, Number>> savedData = DataStorage.getChartData();

        // Dodajemy zapisane dane do wykresu tylko jeśli istnieją
        if (!savedData.isEmpty()) {
            accelerometerSeries.getData().addAll(savedData);
        }

        // Uruchomienie subskrypcji danych
        startDataSubscription();
    }

    private void startDataSubscription() {
        // Upewnij się, że subskrypcja jest zawsze aktywna
        Thread dataVisualizationThread = new Thread(() -> {
            while (true) {
                try {
                    Thread.sleep(DATA_UPDATE_INTERVAL); // Aktualizacje co 200 ms

                    // Pobranie nowych danych
                    List<String> newData = DataStorage.getNewData();
                    if (!newData.isEmpty()) {
                        Platform.runLater(() -> newData.forEach(this::updateChart));
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
            // Rozdzielenie danych na komponenty
            String[] components = data.split(",");
            if (components.length == 4) {
                double x = Double.parseDouble(components[0]);
                double y = Double.parseDouble(components[1]);
                double z = Double.parseDouble(components[2]);
                long timestamp = Long.parseLong(components[3]);

                // Obliczenie przyspieszenia
                double acceleration = Math.sqrt(x * x + y * y + z * z);
                XYChart.Data<Number, Number> dataPoint = new XYChart.Data<>(timestamp, acceleration);

                // Dodaj punkt do serii
                accelerometerSeries.getData().add(dataPoint);

                // Dodajemy dane wykresu do DataStorage
                DataStorage.addChartData(dataPoint);

                // Usuwanie najstarszych danych, jeśli liczba punktów przekracza limit
                if (accelerometerSeries.getData().size() > MAX_VISIBLE_POINTS) {
                    accelerometerSeries.getData().remove(0);
                }

                // Zaktualizuj zakres osi X
                adjustXAxis();

                // Wymuszenie płynnej aktualizacji wykresu tylko raz na cykl
                Platform.runLater(accelerationChart::layout);
            }
        } catch (Exception e) {
            System.err.println("Błąd przetwarzania danych: " + data);
            e.printStackTrace();
        }
    }

    private void adjustXAxis() {
        // Ustaw zakres osi X na podstawie widocznych danych
        if (!accelerometerSeries.getData().isEmpty()) {
            double minX = accelerometerSeries.getData().get(0).getXValue().doubleValue();
            double maxX = accelerometerSeries.getData().get(accelerometerSeries.getData().size() - 1).getXValue().doubleValue();

            NumberAxis xAxis = (NumberAxis) accelerationChart.getXAxis();
            xAxis.setLowerBound(minX);
            xAxis.setUpperBound(maxX);
        }
    }

    @FXML
    public void analyzeGapsInData() {

        gapDetectionOutput.clear();
        // Pobranie progu z bazy danych dla zalogowanego użytkownika
        String thresholdString = UserDAO.getEspExitThresholdForUser(LoginController.loggedInUser);
        double threshold = (thresholdString != null) ? Double.parseDouble(thresholdString) : 0.0;

        if (threshold == 0.0) {
            gapDetectionOutput.appendText("No acceleration threshold is set for the user.\n");
            return;
        }

        // Lista danych z wykresu
        List<XYChart.Data<Number, Number>> data = accelerometerSeries.getData();

        // Sprawdzenie, czy mamy wystarczającą liczbę punktów
        if (data.size() < 3) {
            gapDetectionOutput.appendText("Insufficient data for analysis. At least 3 points are required.\n");
            return;
        }

        System.out.println("Analyzing second-order acceleration differences...");

        boolean foundAtLeastOneGap = false;
        // Iteracja od drugiego punktu do przedostatniego (bo potrzebujemy i-1, i, i+1)
        for (int i = 1; i < data.size() - 1; i++) {
            // Pobranie przyspieszeń dla punktów i-1, i, i+1
            double a_i_minus_1 = data.get(i - 1).getYValue().doubleValue();
            double a_i = data.get(i).getYValue().doubleValue();
            double a_i_plus_1 = data.get(i + 1).getYValue().doubleValue();

            // Obliczanie różnic przyspieszeń
            double increase = a_i_plus_1 - a_i; // Wzrost między i+1 a i
            double decrease = a_i - a_i_minus_1; // Spadek między i a i-1

            // Obliczanie Δ²a
            double delta2 = increase - decrease;

            // Warunki wykrycia "dziury"
            if (increase < 0 && decrease > 0 && Math.abs(delta2) > threshold) {
                long timestamp = data.get(i).getXValue().longValue(); // Pobranie timestampu
                String gapMessage = "Gap detected! Timestamp: " + timestamp + "\n";
                gapDetectionOutput.appendText(gapMessage);
                foundAtLeastOneGap = true;
            }
        }
        if(!foundAtLeastOneGap) {
            gapDetectionOutput.appendText("No gaps found.\n");
        }
    }





    @FXML
    public void handleBackToDashboard() {
        try {
            FXMLLoader loader = new FXMLLoader(getClass().getResource("/dashboard.fxml"));
            AnchorPane dashboard = loader.load();

            Stage stage = (Stage) accelerationChart.getScene().getWindow();
            Scene scene = new Scene(dashboard, 1600, 1200);
            stage.setScene(scene);
            stage.show();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
