package main

import (
	"context"
	"errors"
	"fmt"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/giakiet05/home-station/server/internal/collector"
	"github.com/giakiet05/home-station/server/internal/config"
	"github.com/giakiet05/home-station/server/internal/handler"
	"github.com/giakiet05/home-station/server/internal/model"
	"github.com/giakiet05/home-station/server/internal/notifier"
)

func main() {
	// Initialize structured logger
	logger := slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{
		Level: slog.LevelInfo,
	}))
	slog.SetDefault(logger)

	// Load configuration
	cfg := config.Load()
	logger.Info("Starting Home Station collector service",
		"port", cfg.Port,
		"serial_port", cfg.SerialPort,
		"baud_rate", cfg.BaudRate,
		"mock_mode", cfg.MockMode)

	// Setup context with cancellation on OS interrupt signals
	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer cancel()

	// Initialize serial collector worker
	serialCollector := collector.NewSerialCollector(cfg, logger)

	// Initialize Telegram components if enabled
	if cfg.TelegramEnabled && cfg.TelegramBotToken != "" && cfg.TelegramChatID != 0 {
		telegramClient := notifier.NewHTTPTelegramClient(cfg.TelegramBotToken, logger)

		alertManager := notifier.NewAlertManager(cfg, telegramClient, logger)
		serialCollector.SetListener(func(t model.Telemetry) {
			alertManager.ProcessTelemetry(ctx, t)
		})

		botListener := notifier.NewBotListener(cfg, telegramClient, serialCollector, logger)
		botListener.Start(ctx)

		logger.Info("Telegram notifications and authorized bot listener active",
			"authorized_chat_id", cfg.TelegramChatID,
			"cooldown_sec", cfg.AlertCooldownSec)
	}

	serialCollector.Start(ctx)

	// Initialize HTTP router
	router := handler.NewRouter(serialCollector)

	// Configure HTTP server
	httpServer := &http.Server{
		Addr:         fmt.Sprintf(":%s", cfg.Port),
		Handler:      router,
		ReadTimeout:  5 * time.Second,
		WriteTimeout: 10 * time.Second,
		IdleTimeout:  60 * time.Second,
	}

	// Start HTTP server in a separate goroutine
	go func() {
		logger.Info("HTTP server listening", "address", httpServer.Addr)
		if err := httpServer.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
			logger.Error("HTTP server failed unexpectedly", "error", err.Error())
			os.Exit(1)
		}
	}()

	// Wait for termination signal
	<-ctx.Done()
	logger.Info("Shutdown signal received, performing graceful cleanup...")

	// Graceful shutdown with 5-second timeout
	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer shutdownCancel()

	if err := httpServer.Shutdown(shutdownCtx); err != nil {
		logger.Error("HTTP server graceful shutdown error", "error", err.Error())
	} else {
		logger.Info("HTTP server stopped gracefully")
	}

	logger.Info("Service terminated cleanly")
}
