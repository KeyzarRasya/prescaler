import logging
import docker

logger = logging.getLogger(__name__)

class DockerManager:
    def __init__(self):
        self.client = docker.from_env()
    
    def register_image(self, image_name):
        self.image = image_name

    def scale(self, amount):
        logger.info("Starting to scale to %s", amount)
        container_info = []
        containers = self.client.containers.list(
            filters={
                'ancestor': self.image
            }
        )
        logger.info("Getting container list %s", len(containers))

        container_len = len(containers)
        container_scale = amount - container_len

        for container in containers:
            ports = container.attrs['NetworkSettings']['Ports']

            for mappings in ports.values():
                container_info.append({
                    'id': container.id,
                    'name': container.name,                    
                    'host_port': mappings[0]['HostPort']
                })

        self.containers = container_info
        requirement = self.compute_requirement(container_scale)
        strategy = requirement['strategy']
        if strategy == "UPSCALING":
            self.upscale(requirement['ports'])
        elif strategy == "DOWNSCALING":
            self.downscale(requirement['targets'])
        else:
            raise ValueError(f"Unrecognize strategy: {strategy}")

    def find_free_ports(self, amount: int, start_port: int = 3000):
        used_ports = {int(c["host_port"]) for c in self.containers}

        free_ports = []
        port = start_port

        while len(free_ports) < amount:
            if port not in used_ports:
                free_ports.append(port)
            port += 1

        logger.info("Successfully find free ports %s", free_ports)
        return free_ports

    def compute_requirement(self, to_scale: int) -> dict:
        requirement = {}

        containers_sorted = sorted(
            self.containers,
            key=lambda x: int(x["host_port"])
        )

        if to_scale < 0:
            requirement["strategy"] = "DOWNSCALING"
            to_remove = abs(to_scale)
            requirement["targets"] = containers_sorted[-to_remove:]
        else:
            requirement["strategy"] = "UPSCALING"
            new_ports = self.find_free_ports(to_scale)
            requirement["ports"] = new_ports
        logger.info("Successfully compute the requirement")
        return requirement
    
    def downscale(self, targets: dict):
        for target in targets:
            self.client.containers.get(target['id']).stop(timeout=10)
        logger.info("Success downscale on target %s", targets)
    

    def upscale(self, ports: list):
        for port in ports:
            self.client.containers.run(
                image=self.image,
                detach=True,
                ports={"3000/tcp": port}
            )
        logger.info("Success Upscaling the following ports %s", ports)

    def run_container(self):
        runner = self.client.containers.run(
                    image=self.image,
                    detach=True,
                )
        print(runner)