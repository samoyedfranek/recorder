import os
import time
from shutil import copy2
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
            for attempt in range(5):
                try:
                    creds.refresh(Request())
                    break  # Exit the loop if successful
                except Exception as e:
                    print(f"Attempt {attempt + 1}: Error refreshing token: {e}")
                    if attempt == 4:  # If final attempt fails
                        print("Proceeding with limited functionality.")
                        return None
                    time.sleep(2 ** attempt)
        
        if not creds:
            print("No valid credentials available. Starting new authentication flow.")
            try:
                flow = InstalledAppFlow.from_client_secrets_file('credentials.json', SCOPES)
                creds = flow.run_local_server(port=0)
            except Exception as e:
                print(f"Authentication flow failed: {e}")
                return None
            
        with open('token.json', 'w') as token:
            token.write(creds.to_json())
    
    return build('drive', 'v3', credentials=creds) if creds else None

def upload_to_google_drive(file_path, folder_id, service):
    """
    Upload a file to Google Drive with retries.
    Returns:
        bool: True if upload was successful, False otherwise.
    """
    file_metadata = {'name': os.path.basename(file_path), 'parents': [folder_id]}
    media = MediaFileUpload(file_path, mimetype='audio/wav')

    for attempt in range(5):  # Retry up to 5 times
        try:
            service.files().create(body=file_metadata, media_body=media, fields='id').execute()
            print(f"File '{file_path}' uploaded to Google Drive successfully.")
            return True
        except Exception as e:
            print(f"Attempt {attempt + 1} to upload file failed: {e}")
            time.sleep(5)
    
    # If all attempts fail, copy the file to a cache folder
    cache_folder = 'cache'
    os.makedirs(cache_folder, exist_ok=True)
    cached_file_path = os.path.join(cache_folder, os.path.basename(file_path))
    try:
        copy2(file_path, cached_file_path)
        print(f"File '{file_path}' copied to cache folder: {cached_file_path}")
    except Exception as e:
        print(f"Failed to copy file to cache folder: {e}")
    
    print(f"Failed to upload '{file_path}' to Google Drive after 5 attempts.")
    return False