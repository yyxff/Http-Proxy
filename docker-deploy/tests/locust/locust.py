from locust import HttpUser, task, between

class WebsiteUser(HttpUser):
    wait_time = between(0.5, 1)  # user wait time

    def on_start(self):
        self.client.proxies = {
            "http": "http://proxy:12345",
            "https": "http://proxy:12345",
        }

    @task()
    def index(self):
        self.client.get("")  # visit the host url
