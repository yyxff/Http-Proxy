FROM python:3.9-slim

WORKDIR /app
COPY . .

RUN pip install flask && \
    chmod 777 /app/CacheServer.py

CMD ["python3", "/app/CacheServer.py"]