# Prescal

Prescal is a comprehensive, multi-component system designed for scalable web services. It includes a lightweight, high-performance reverse proxy and load balancer written in C, a real-time monitoring dashboard built with Svelte, an event handling service in Go, and a traffic generation tool for testing.

## Components

The project is divided into the following main components:

- **`prescal`**: The core of the system, this C-based application acts as a reverse proxy and load balancer. It is designed for high performance using `epoll` for asynchronous I/O. A key feature is its ability to dynamically scale backend services by interacting with the Docker daemon to launch new container instances based on traffic load.

- **`dashboard`**: A modern, real-time web dashboard built with Svelte and Vite. It provides a user interface to monitor the status and performance of the `prescal` server and the backend services it manages.

- **`event`**: A Go-based service that handles events within the system. It utilizes Redis Pub/Sub to receive and process messages, likely for metrics, logging, or other asynchronous tasks.

- **`traffic`**: A utility for generating HTTP traffic to test the load balancing and auto-scaling capabilities of the `prescal` server. It is written in C and can simulate a configurable number of concurrent requests.

## Architecture Overview

The components of Prescal work together to create a robust and scalable system:

1.  The **`traffic`** generator is used to send a configurable amount of HTTP requests to the **`prescal`** server.
2.  The **`prescal`** server, acting as a reverse proxy, receives the incoming requests and distributes them among the available backend services.
3.  **`prescal`** monitors the traffic load and, if necessary, communicates with the Docker daemon to start or stop backend container instances to match the current demand.
4.  The **`event`** service subscribes to a Redis channel where **`prescal`** publishes events and metrics. This allows for real-time monitoring and event processing.
5.  The **`dashboard`** provides a web-based interface to visualize the data from the **`event`** service, offering a real-time view of the system's health and performance.

## Getting Started

To get the full Prescal system up and running, you will need to build and run each component.

### Prerequisites

- [Docker](https://www.docker.com/)
- [Go](https://golang.org/)
- [Node.js](https://nodejs.org/) and [npm](https://www.npmjs.com/)
- A C compiler (like GCC)
- [CMake](https://cmake.org/)
- [Redis](https://redis.io/)

### Building and Running

1.  **`prescal`** (C Application)
    ```bash
    cd prescal
    mkdir build
    cd build
    cmake ..
    make
    ./prescal
    ```

2.  **`dashboard`** (Svelte Application)
    ```bash
    cd dashboard
    npm install
    npm run dev
    ```

3.  **`event`** (Go Application)
    ```bash
    cd event
    go run main.go
    ```

4.  **`traffic`** (C Application)
    ```bash
    cd traffic
    mkdir build
    cd build
    cmake ..
    make
    ./traffic
    ```

## Configuration

Each component may have its own configuration file. Refer to the `README.md` file within each component's directory for more detailed instructions on configuration.

- **`prescal`**: `prescal/config.yml`
- **`dashboard`**: `dashboard/svelte.config.js`
- **`event`**: The Redis connection is configured in `event/main.go`.

---
*This README provides a general overview of the Prescal project. For more detailed information, please refer to the `README.md` files in the respective component directories.*
