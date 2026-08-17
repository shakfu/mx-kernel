{
 "patcher": {
  "fileversion": 1,
  "appversion": {
   "major": 9,
   "minor": 1,
   "revision": 2,
   "architecture": "x64",
   "modernui": 1
  },
  "classnamespace": "box",
  "rect": [
   100.0,
   100.0,
   800.0,
   540.0
  ],
  "boxes": [
   {
    "box": {
     "id": "c-1",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20.0,
      10.0,
      560.0,
      20.0
     ],
     "text": "mx-kernel test rig -- see source/projects/kernel/README.md walkthrough"
    }
   },
   {
    "box": {
     "id": "c-2",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20.0,
      30.0,
      560.0,
      20.0
     ],
     "text": "1. Send 'start'. 2. Terminal: make connect NAME=maxtest"
    }
   },
   {
    "box": {
     "id": "m-start",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20.0,
      60.0,
      40.0,
      22.0
     ],
     "text": "start",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "m-stop",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      70.0,
      60.0,
      40.0,
      22.0
     ],
     "text": "stop",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "m-info",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      120.0,
      60.0,
      40.0,
      22.0
     ],
     "text": "info",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "c-3",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      175.0,
      62.0,
      380.0,
      20.0
     ],
     "text": "step 8: stop then start again -- must come back up"
    }
   },
   {
    "box": {
     "id": "k",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 2,
     "patching_rect": [
      20.0,
      300.0,
      290.0,
      22.0
     ],
     "text": "kernel @name maxtest @debug 1 @timeout 10",
     "outlettype": [
      "",
      ""
     ]
    }
   },
   {
    "box": {
     "id": "c-4",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20.0,
      100.0,
      400.0,
      20.0
     ],
     "text": "Echo loop. Toggle OFF to test the MaxTimeout path (step 3)."
    }
   },
   {
    "box": {
     "id": "t-gate",
     "maxclass": "toggle",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      20.0,
      122.0,
      24.0,
      24.0
     ],
     "text": "",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "route-code",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      20.0,
      155.0,
      80.0,
      22.0
     ],
     "text": "route code",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "route-exec",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      20.0,
      185.0,
      95.0,
      22.0
     ],
     "text": "route execute",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "gate",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20.0,
      215.0,
      60.0,
      22.0
     ],
     "text": "gate",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "prep",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      20.0,
      245.0,
      95.0,
      22.0
     ],
     "text": "prepend result",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "c-5",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      330.0,
      100.0,
      420.0,
      20.0
     ],
     "text": "step 4/5: streaming then a result (send top to bottom while a cell waits)"
    }
   },
   {
    "box": {
     "id": "m-print1",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      330.0,
      122.0,
      130.0,
      22.0
     ],
     "text": "print working on it",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "m-print2",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      330.0,
      148.0,
      210.0,
      22.0
     ],
     "text": "print stderr something looks wrong",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "m-result",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      330.0,
      174.0,
      90.0,
      22.0
     ],
     "text": "result done",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "m-err",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      330.0,
      200.0,
      225.0,
      22.0
     ],
     "text": "result error MaxError no such object",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "c-6",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      330.0,
      228.0,
      440.0,
      20.0
     ],
     "text": "step 6: send this with NO cell running -- must not become the next Out[n]"
    }
   },
   {
    "box": {
     "id": "m-stale",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      330.0,
      250.0,
      120.0,
      22.0
     ],
     "text": "result i am stale",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "c-7",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      330.0,
      278.0,
      440.0,
      20.0
     ],
     "text": "step 11: with no cell running, this should reach the client immediately"
    }
   },
   {
    "box": {
     "id": "m-idle",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      330.0,
      300.0,
      150.0,
      22.0
     ],
     "text": "print hello while idle",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "c-8",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20.0,
      340.0,
      420.0,
      20.0
     ],
     "text": "step 7: click 'replace...' to fill the dict, then 'dict mydict'"
    }
   },
   {
    "box": {
     "id": "c-9",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20.0,
      360.0,
      460.0,
      20.0
     ],
     "text": "(if 'replace' is not a dict message in your Max, use the dict help to fill it)"
    }
   },
   {
    "box": {
     "id": "m-fill",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20.0,
      382.0,
      120.0,
      22.0
     ],
     "text": "replace freq 440",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "m-fill2",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      150.0,
      382.0,
      125.0,
      22.0
     ],
     "text": "replace name osc1",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "d",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 4,
     "patching_rect": [
      20.0,
      412.0,
      90.0,
      22.0
     ],
     "text": "dict mydict",
     "outlettype": [
      "",
      "",
      "",
      ""
     ]
    }
   },
   {
    "box": {
     "id": "m-dict",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      285.0,
      382.0,
      85.0,
      22.0
     ],
     "text": "dict mydict",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "p-out",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20.0,
      450.0,
      145.0,
      22.0
     ],
     "text": "print output @popup 1"
    }
   },
   {
    "box": {
     "id": "p-status",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      200.0,
      450.0,
      140.0,
      22.0
     ],
     "text": "print status @popup 1"
    }
   }
  ],
  "lines": [
   {
    "patchline": {
     "destination": [
      "k",
      0
     ],
     "source": [
      "m-start",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "k",
      0
     ],
     "source": [
      "m-stop",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "k",
      0
     ],
     "source": [
      "m-info",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "route-code",
      0
     ],
     "source": [
      "k",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "route-exec",
      0
     ],
     "source": [
      "route-code",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "gate",
      1
     ],
     "source": [
      "route-exec",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "gate",
      0
     ],
     "source": [
      "t-gate",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "prep",
      0
     ],
     "source": [
      "gate",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "k",
      0
     ],
     "source": [
      "prep",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "k",
      0
     ],
     "source": [
      "m-print1",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "k",
      0
     ],
     "source": [
      "m-print2",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "k",
      0
     ],
     "source": [
      "m-result",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "k",
      0
     ],
     "source": [
      "m-err",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "k",
      0
     ],
     "source": [
      "m-stale",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "k",
      0
     ],
     "source": [
      "m-idle",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "d",
      0
     ],
     "source": [
      "m-fill",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "d",
      0
     ],
     "source": [
      "m-fill2",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "k",
      0
     ],
     "source": [
      "m-dict",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "p-out",
      0
     ],
     "source": [
      "k",
      0
     ]
    }
   },
   {
    "patchline": {
     "destination": [
      "p-status",
      0
     ],
     "source": [
      "k",
      1
     ]
    }
   }
  ],
  "autosave": 0
 }
}