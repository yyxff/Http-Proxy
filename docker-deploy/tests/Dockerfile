FROM python:3.9-slim

WORKDIR /app

RUN pip install pytest && \
    pip install requests

COPY . .

CMD ["pytest", "-v", "/app/Tester.py"]