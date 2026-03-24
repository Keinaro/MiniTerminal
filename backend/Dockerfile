FROM node:20-bookworm-slim

WORKDIR /srv

RUN apt-get update \
    && apt-get install -y --no-install-recommends g++ \
    && rm -rf /var/lib/apt/lists/*

COPY backend/package*.json /srv/backend/
RUN cd /srv/backend && npm install --omit=dev

COPY backend/server.js /srv/backend/server.js
COPY terminal.cpp /srv/terminal.cpp

RUN g++ -std=c++17 /srv/terminal.cpp -o /srv/mi_terminal

EXPOSE 3001

CMD ["node", "/srv/backend/server.js"]
