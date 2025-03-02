from locust import HttpUser, task, between

class WebsiteUser(HttpUser):
    wait_time = between(0.5, 1)  # user wait time

    def on_start(self):
        self.client.proxies = {
            "http": "http://localhost:12345",
            "https": "http://localhost:12345",
        }

    @task(4)
    def index(self):
        self.client.get("/")  # four times for index visit
