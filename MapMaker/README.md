This project was created and built on Processing (https://processing.org/)

This is a map creator GUI that allows the user to choose objects using the 
number keys and place them around a custom sized tile map, and then press
enter to generate a json object that represents that map.

Example JSON object:

{
  "obstacles": {
    "rock": [
      [
        3.5, 4.5
      ],
      [
        8.5, 4.5
      ],
      [
        4.5, 8.5
      ],
      [
        10.5, 10.5
      ],
      [
        5.5, 2.5
      ]
    ],
    "tree": [
      [
        2.5, 7.5
      ],
      [
        7.5, 7.5
      ],
      [
        9.5, 1.5
      ],
      [
        1.5, 2.5
      ],
      [
        3.5, 10.5
      ]
    ]
  },
  "config": {
    "num_rows": 12,
    "num_cols": 12
  }
}
