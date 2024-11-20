import os
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from google.auth.transport.requests import Request
from googleapiclient.discovery import build
from googleapiclient.http import MediaFileUpload

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
    """Upload a file to Google Drive in a specific folder."""
    file_metadata = {
        'name': os.path.basename(file_path),
        'parents': [folder_id],  # Specify the folder to save the file
    }
    media = MediaFileUpload(file_path, mimetype='audio/wav')
    try:
        service.files().create(body=file_metadata, media_body=media, fields='id').execute()
        print(f"File '{file_path}' uploaded to Google Drive successfully.")
    except Exception as e:
        print(f"Failed to upload file: {e}")
