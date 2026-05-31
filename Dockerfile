# Use an official Python runtime as a parent image
FROM python:3.9-slim

# Set the working directory in the container
WORKDIR /app

# Copy the current directory contents into the container at /app
COPY . /app

# The broker only uses standard library, so no requirements.txt needed yet.
# If we add dependencies later, we'd run:
# RUN pip install --no-cache-dir -r requirements.txt

# Expose the ports the broker uses
EXPOSE 5000 8080

# Define environment variables
ENV WEB_PORT=5000
ENV TCP_PORT=8080
ENV DB_PATH=/app/data/platform.db
ENV BIND_ADDR=0.0.0.0

# Create data directory for persistent DB
RUN mkdir -p /app/data

# Run setup_db.py if the DB doesn't exist, then start the broker
CMD ["sh", "-c", "if [ ! -f $DB_PATH ]; then python3 setup_db.py; fi && python3 broker.py"]
