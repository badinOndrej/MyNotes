1.  - From the Releases section download the version compiled for your device's firmware and hardware level

    - Alternatively clone the repository and build from source

2.  Copy notes_app.app to the `applications` folder on the device

3.  Optionally for desktop integration

    -   Copy the `icons` folder to the `applications` folder on the device, merge if it already exists

    -   put the following in `system/config/desktop/view.json`
    
        -   between the `applications` and `_comment` keys

            ```
            "U_NotesApp": {
                "path": "/mnt/ext1/applications/notes_app.app",
                "title": "MyNotes",
                "icon": "/mnt/ext1/applications/icons/notes_app.bmp",
                "focused_icon": "/mnt/ext1/applications/icons/notes_app_f.bmp"
            },
            ```
            e.g.

            ```
            {
                "version": "5",
                "applications": {
                    "U_NotesApp": {
                        "path": "/mnt/ext1/applications/notes_app.app",
                        "title": "MyNotes",
                        "icon": "/mnt/ext1/applications/icons/notes_app.bmp",
                        "focused_icon": "/mnt/ext1/applications/icons/notes_app_f.bmp"
                    },
                    "_comment": [
                        "Fields - application key id, must start from \"U_\" for user application\n",
                        "Support application subfields:\n",
            ```

        -   in one of the application groups, e.g. @General

            ```
            "U_NotesApp"
            ```
            e.g.

            ```
            "groups": [{
                            "title": "@General",
                            "sort": "title",
                            "apps": [
                                "PB_Dictionary",
                                "PB_Library",
                                "PB_Settings",
                                "U_NotesApp"
                            ]
                        },
            ```

