FROM python:3.9-slim

WORKDIR /app

RUN pip install flask 

COPY . .

CMD ["python3", "/app/CacheServer.py"]