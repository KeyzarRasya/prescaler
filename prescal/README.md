# Prescal

Prescal is a simple, lightweight, and scalable reverse proxy and load balancer written in C. It is designed to dynamically scale backend services by interacting with the Docker daemon to start new container instances based on incoming traffic.

## Features

*   **Dynamic Backend Scaling:** Automatically starts new backend instances as Docker containers to handle increased load.
*   **Load Balancing:** Distributes incoming requests across a pool of available backend servers.
*   **HTTP Proxying:** Forwards HTTP requests to the appropriate backend service.
*   **Configurable:** Easy to configure through a simple configuration file.

## How it Works

Prescal listens for incoming HTTP requests on a specified port. When a request is received, it forwards it to one of the backend servers in its pool. The pool of backend servers is managed as a linked list. Prescal can be configured to monitor the load and, when necessary, communicate with the Docker daemon to start new containerized backend instances, adding them to the pool of available servers.

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

### Prerequisites

*   [CMake](https://cmake.org/)
*   A C compiler (like GCC)
*   [Docker](https://www.docker.com/)

### Building

1.  Clone the repository:
    ```bash
    git clone https://github.com/your_username/prescal.git
    cd prescal
    ```

2.  Create a build directory and run CMake:
    ```bash
    mkdir build
    cd build
    cmake ..
    ```

3.  Compile the project:
    ```bash
    make
    ```

### Running

Once the project is built, you can run the main executable:

```bash
./prescal
```

## Configuration

Prescal can be configured by editing the `config.h` file or by providing a configuration file. The configuration options include:

*   The port for the proxy to listen on.
*   The Docker image to use for new backend instances.
*   Parameters for scaling (e.g., number of requests before scaling up).

*This README is a starting point. Feel free to edit and expand it as the project grows.*
