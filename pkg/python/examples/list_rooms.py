"""Read-only room listing using LIVEKIT_URL and LIVEKIT_API_KEY/SECRET or LIVEKIT_TOKEN.

Build the SDK shared library and set LIVEKIT_SERVER_PYTHON_LIBRARY before running.
"""
from livekit_server import LiveKitApi


def main():
    response = LiveKitApi().room.list_rooms()
    for room in response.to_dict().get("rooms", []):
        print(room.get("name", ""))


if __name__ == "__main__":
    main()
