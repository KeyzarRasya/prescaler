# ---------- Stage 1: Build ----------
FROM node:20-slim AS builder

# Install only essential build deps
RUN apt-get update && apt-get install -y --no-install-recommends \
  ca-certificates \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY package*.json ./
RUN npm ci --omit=dev=false

COPY . .
RUN npm run build

# ---------- Stage 2: Runtime ----------
FROM node:20-slim

WORKDIR /app

ENV NODE_ENV=production

# Copy only what's needed for runtime
COPY --from=builder /app/package*.json ./
COPY --from=builder /app/build ./build

# Install only production deps
RUN npm ci --omit=dev && \
    apt-get purge -y --auto-remove && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

EXPOSE 3000

CMD ["node", "build"]
