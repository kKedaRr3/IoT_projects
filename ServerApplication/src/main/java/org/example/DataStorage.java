package org.example;

import javafx.scene.chart.XYChart;

import java.util.ArrayList;
import java.util.List;

public class DataStorage {
    private static final List<String> accelerometerData = new ArrayList<>();
    private static final List<XYChart.Data<Number, Number>> chartData = new ArrayList<>();
    private static int lastFetchedIndex = 0;

    // Dodawanie danych
    public static synchronized void addData(String data) {
        accelerometerData.add(data);
    }

    // Pobieranie nowych danych od ostatniego odczytu
    public static synchronized List<String> getNewData() {
        List<String> newData = accelerometerData.subList(lastFetchedIndex, accelerometerData.size());
        lastFetchedIndex = accelerometerData.size(); // Aktualizuj wskaźnik
        return new ArrayList<>(newData); // Zwróć kopię nowych danych
    }

    public static synchronized List<String> getAllData() {
        return accelerometerData; // Zwróć kopię nowych danych
    }

    // Dodawanie punktów do wykresu
    public static synchronized void addChartData(XYChart.Data<Number, Number> dataPoint) {
        chartData.add(dataPoint);
    }

    // Pobieranie wszystkich punktów wykresu
    public static synchronized List<XYChart.Data<Number, Number>> getChartData() {
        return new ArrayList<>(chartData);
    }

    // Czyszczenie danych (opcjonalnie)
    public static synchronized void reset() {
        accelerometerData.clear();
        chartData.clear();
        lastFetchedIndex = 0;
    }
}
