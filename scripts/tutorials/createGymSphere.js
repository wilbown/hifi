//
//  Created by Wil Bown
//
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

var SCRIPT_URL =  "file:///home/wilbown/GitHub/hifi/scripts/tutorials/entity_scripts/gymSphere.js";
var center = Vec3.sum(MyAvatar.position, Vec3.multiply(0.5, Quat.getForward(Camera.getOrientation())));

var BALL_GRAVITY = {
    x: 0,
    y: 0,
    z: 0
};
    
var BALL_DIMENSIONS = {
    x: 0.4,
    y: 0.4,
    z: 0.4
};


var BALL_COLOR = {
    red: 255,
    green: 0,
    blue: 0
};

var gymSphereProperties = {
    name: 'Gym Sphere',
    shapeType: 'sphere',
    type: 'Sphere',
    serverScripts: SCRIPT_URL,
    color: BALL_COLOR,
    dimensions: BALL_DIMENSIONS,
    gravity: BALL_GRAVITY,
    dynamic: false,
    position: center,
    collisionless: false,
    ignoreForCollisions: true
};

var gymSphere = Entities.addEntity(gymSphereProperties);

Script.stop();
