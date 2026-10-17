# Destructor regression

Human review: pending. Expected output was independently traced by AI.

Explicit this.DELETE prints only the method. DELETE Ptr^ and DELETE this release heap objects; deleting an array element fails validation. Main destroys the array in reverse and original Obj storage.
