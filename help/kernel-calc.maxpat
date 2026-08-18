{
    "patcher": {
        "fileversion": 1,
        "appversion": {
            "major": 9,
            "minor": 1,
            "revision": 5,
            "architecture": "x64",
            "modernui": 1
        },
        "classnamespace": "box",
        "rect": [ 120.0, 120.0, 695.0, 466.0 ],
        "boxes": [
            {
                "box": {
                    "fontface": 1,
                    "id": "c1",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 29.0, 21.0, 414.0, 20.0 ],
                    "text": "mx-kernel calculator example: a patch that evaluates, not echoes"
                }
            },
            {
                "box": {
                    "id": "c2",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 29.0, 61.0, 520.0, 20.0 ],
                    "text": "1. Send 'start'   2. Terminal: make connect NAME=calc"
                }
            },
            {
                "box": {
                    "id": "c3",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 29.0, 81.0, 520.0, 20.0 ],
                    "text": "Then try:  1+1   |   2+3*4   |   sqrt(pow(3,2)+pow(4,2))   |   pi*2"
                }
            },
            {
                "box": {
                    "id": "c4",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 29.0, 101.0, 520.0, 20.0 ],
                    "text": "Errors fail the cell properly:  1/0   |   1+   |   bar"
                }
            },
            {
                "box": {
                    "id": "m-start",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 136.0, 261.0, 40.0, 22.0 ],
                    "text": "start"
                }
            },
            {
                "box": {
                    "id": "m-stop",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 193.0, 261.0, 40.0, 22.0 ],
                    "text": "stop"
                }
            },
            {
                "box": {
                    "id": "k",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "" ],
                    "patching_rect": [ 29.0, 311.0, 265.0, 22.0 ],
                    "text": "kernel @name calc @debug 1 @timeout 10"
                }
            },
            {
                "box": {
                    "id": "c5",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 29.0, 136.0, 424.5, 20.0 ],
                    "text": "The cell text arrives as the message selector after [route execute],"
                }
            },
            {
                "box": {
                    "id": "c6",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 32.0, 154.0, 422.0, 20.0 ],
                    "text": "so calc.js rebuilds it and emits a complete 'result ...' message."
                }
            },
            {
                "box": {
                    "id": "route-code",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 2,
                    "outlettype": [ "", "" ],
                    "patching_rect": [ 29.0, 191.0, 80.0, 22.0 ],
                    "text": "route code"
                }
            },
            {
                "box": {
                    "id": "route-exec",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 2,
                    "outlettype": [ "", "" ],
                    "patching_rect": [ 29.0, 226.0, 95.0, 22.0 ],
                    "text": "route execute"
                }
            },
            {
                "box": {
                    "id": "js",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 29.0, 261.0, 80.0, 22.0 ],
                    "saved_object_attributes": {
                        "filename": "calc.js",
                        "parameter_enable": 0
                    },
                    "text": "js calc.js"
                }
            },
            {
                "box": {
                    "id": "p-out",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 29.0, 412.0, 145.0, 22.0 ],
                    "text": "print output @popup 1"
                }
            },
            {
                "box": {
                    "id": "p-status",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 275.0, 407.0, 140.0, 22.0 ],
                    "text": "print status @popup 1"
                }
            },
            {
                "box": {
                    "id": "c7",
                    "linecount": 3,
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 48.0, 342.0, 194.0, 47.0 ],
                    "text": "js calc.js emits:   result <value>   or   result error <name> <message>"
                }
            },
            {
                "box": {
                    "id": "c8",
                    "linecount": 2,
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 306.0, 336.0, 217.0, 33.0 ],
                    "text": "No eval() -- calc.js parses the expression, so a cell cannot run code."
                }
            }
        ],
        "lines": [
            {
                "patchline": {
                    "destination": [ "k", 0 ],
                    "source": [ "js", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "p-out", 0 ],
                    "order": 0,
                    "source": [ "k", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "p-status", 0 ],
                    "source": [ "k", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "route-code", 0 ],
                    "midpoints": [ 38.5, 335.0, 15.0, 335.0, 15.0, 182.0, 38.5, 182.0 ],
                    "order": 1,
                    "source": [ "k", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "k", 0 ],
                    "source": [ "m-start", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "k", 0 ],
                    "source": [ "m-stop", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "route-exec", 0 ],
                    "source": [ "route-code", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "js", 0 ],
                    "source": [ "route-exec", 0 ]
                }
            }
        ],
        "autosave": 0
    }
}