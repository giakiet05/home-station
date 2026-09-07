# Build stage
FROM golang:1.24-alpine AS builder

ENV GOTOOLCHAIN=auto
WORKDIR /app

# Copy entire context
COPY . .

# Dynamically handle both root context and server/ subfolder context
RUN if [ -f "./go.mod" ]; then \
        go mod download && CGO_ENABLED=0 GOOS=linux go build -ldflags="-s -w" -o /app/home-station-server ./cmd/server; \
    elif [ -f "./server/go.mod" ]; then \
        cd server && go mod download && CGO_ENABLED=0 GOOS=linux go build -ldflags="-s -w" -o /app/home-station-server ./cmd/server; \
    fi

# Production runtime stage
FROM alpine:3.21

RUN apk --no-cache add ca-certificates tzdata

WORKDIR /app
COPY --from=builder /app/home-station-server /app/home-station-server

EXPOSE 8080

ENTRYPOINT ["/app/home-station-server"]
