import firebase_admin
from firebase_admin import credentials, storage, db
import librosa
import numpy as np
from tensorflow.keras.models import load_model
import time

# Check if Firebase app is already initialized
if not firebase_admin._apps:
    # Initialize Firebase Admin SDK
    cred = credentials.Certificate('D:/project_data/alerty-b8707-firebase-adminsdk-2p77b-b00f425f74.json')
    firebase_admin.initialize_app(cred, {
        'storageBucket': 'alerty-b8707.appspot.com',
        'databaseURL': 'https://alerty-b8707-default-rtdb.europe-west1.firebasedatabase.app'  # Corrected database URL
    })

# Access Firebase Storage
bucket = storage.bucket()

# Get a database reference to the root
root_ref = db.reference()
db_ref = root_ref.child('predictions')  # Reference to the database node for predictions

# Loop to continuously check for new files
while True:
    blobs = bucket.list_blobs()
    for blob in blobs:
        if blob.name.endswith('.wav'):
            # Download the audio file directly from Firebase Storage
            temp_local_filename = 'D:/project_data/firebase_downloads/temp_audio.wav'
            blob.download_to_filename(temp_local_filename)

            # Load the pre-trained model
            model_path = 'D:/new_ai/PRE-TRAINED_AI_MODEL_FINAL.h5'
            loaded_model = load_model(model_path)

            # Load the downloaded audio file for testing
            audio, sample_rate = librosa.load(temp_local_filename, res_type='kaiser_fast')
            mfccs_features = librosa.feature.mfcc(y=audio, sr=sample_rate, n_mfcc=40)
            mfccs_scaled_features = np.mean(mfccs_features.T, axis=0)
            mfccs_scaled_features = mfccs_scaled_features.reshape(1, -1)

            # Make predictions
            predicted_probabilities = loaded_model.predict(mfccs_scaled_features)
            predicted_label = np.argmax(predicted_probabilities, axis=1)

            # Mapping of numerical labels to class names
            class_mapping = {
                0: 'air_conditioner',
                1: 'car_horn',
                2: 'children_playing',
                3: 'dog_bark',
                4: 'drilling',
                5: 'engine_idling',
                6: 'gun_shot',
                7: 'jackhammer',
                8: 'siren',
                9: 'street_music'
            }

            # Get the predicted class name based on the numerical label
            predicted_class_name = class_mapping[predicted_label[0]]
            print(f"Predicted class: {predicted_class_name}")

            # Write predicted class name to the real-time database (overwrite existing data)
            db_ref.set({'prediction': predicted_class_name})  # Overwrite the existing data

            # Optionally, remove the temporary downloaded file
            # import os
            # os.remove(temp_local_filename)
    
    # Wait for a specified time before checking again
    time.sleep(40)  # Check every minute, adjust as needed
