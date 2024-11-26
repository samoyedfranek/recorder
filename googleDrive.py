import os
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from google.auth.transport.requests import Request
from googleapiclient.discovery import build
from googleapiclient.http import MediaFileUpload
import time
# Google Drive API scopes
SCOPES = ['https://www.googleapis.com/auth/drive.file']

def authenticate_google_drive():
    """Authenticate and return a Google Drive service instance."""
    creds = None
    if os.path.exists('token.json'):
        creds = Credentials.from_authorized_user_file('token.json', SCOPES)
    if not creds or not creds.valid:
        if creds and creds.expired and creds.refresh_token:
            creds.refresh(Request())
        else:
            flow = InstalledAppFlow.from_client_secrets_file('credentials.json', SCOPES)
            creds = flow.run_local_server(port=0)
        # Save the credentials for future use
        with open('token.json', 'w') as token:
            token.write(creds.to_json())
    return build('drive', 'v3', credentials=creds)

def upload_to_google_drive(file_path, folder_id, service):
    """Upload a file to Google Drive with retries."""
    file_metadata = {'name': os.path.basename(file_path), 'parents': [folder_id]}
    media = MediaFileUpload(file_path, mimetype='audio/wav')

    for attempt in range(5):  # Retry up to 3 times
        try:
            service.files().create(body=file_metadata, media_body=media, fields='id').execute()
            print(f"File '{file_path}' uploaded to Google Drive successfully.")
            return
        except Exception as e:
            print(f"Attempt {attempt + 1} to upload file failed: {e}")
            time.sleep(5)
    print(f"Failed to upload '{file_path}' to Google Drive after 5 attempts.")
