//
//  light_modifier.js
//
//  Created byJames Pollack @imgntn on 10/19/2015
//  Copyright 2015 High Fidelity, Inc.
//
//  Given a selected light, instantiate some entities that represent various values you can dynamically adjust by grabbing and moving.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

var AXIS_SCALE = 1;
var COLOR_MAX = 255;
var INTENSITY_MAX = 0.05;
var CUTOFF_MAX = 360;
var EXPONENT_MAX = 1;

var SLIDER_SCRIPT_URL = Script.resolvePath('slider.js?' + Math.random(0, 100));

var RED = {
    red: 255,
    green: 0,
    blue: 0
};

var GREEN = {
    red: 0,
    green: 255,
    blue: 0
};

var BLUE = {
    red: 0,
    green: 0,
    blue: 255
};

var PURPLE = {
    red: 255,
    green: 0,
    blue: 255
};

var WHITE = {
    red: 255,
    green: 255,
    blue: 255
};

var SLIDER_DIMENSIONS = {
    x: 0.075,
    y: 0.075,
    z: 0.075
};

var PER_ROW_OFFSET = {
    x: 0,
    y: -0.2,
    z: 0
};

function entitySlider(light, color, sliderType, row) {
    this.light = light;
    this.lightID = light.id.replace(/[{}]/g, "");
    this.initialProperties = light.initialProperties;
    this.color = color;
    this.sliderType = sliderType;
    this.verticalOffset = Vec3.multiply(row, PER_ROW_OFFSET);
    this.avatarRot = Quat.fromPitchYawRollDegrees(0, MyAvatar.bodyYaw, 0.0);
    this.basePosition = Vec3.sum(MyAvatar.position, Vec3.multiply(1.5, Quat.getFront(this.avatarRot)));

    var message = {
        lightID: this.lightID,
        sliderType: this.sliderType,
        sliderValue: null
    };

    if (this.sliderType === 'color_red') {
        message.sliderValue = this.initialProperties.color.red
        this.setValueFromMessage(message);
    }
    if (this.sliderType === 'color_green') {
        message.sliderValue = this.initialProperties.color.green
        this.setValueFromMessage(message);
    }
    if (this.sliderType === 'color_blue') {
        message.sliderValue = this.initialProperties.color.blue
        this.setValueFromMessage(message);
    }

    if (this.sliderType === 'intensity') {
        message.sliderValue = this.initialProperties.intensity
        this.setValueFromMessage(message);
    }

    if (this.sliderType === 'exponent') {
        message.sliderValue = this.initialProperties.exponent
        this.setValueFromMessage(message);
    }

    if (this.sliderType === 'cutoff') {
        message.sliderValue = this.initialProperties.cutoff
        this.setValueFromMessage(message);
    }

    this.setInitialSliderPositions();
    this.createAxis();
    this.createSliderIndicator();
    return this;
}

//what's the ux for adjusting values?  start with simple entities, try image overlays etc
entitySlider.prototype = {
    createAxis: function() {
        //start of line
        var position = Vec3.sum(this.basePosition, this.verticalOffset);

        //line starts on left and goes to right
        //set the end of the line to the right
        var rightVector = Quat.getRight(this.avatarRot);
        var extension = Vec3.multiply(AXIS_SCALE, rightVector);
        var endOfAxis = Vec3.sum(position, extension);
        this.endOfAxis = endOfAxis;
        var properties = {
            type: 'Line',
            name: 'Hifi-Slider-Axis::' + this.sliderType,
            color: this.color,
            collisionsWillMove: false,
            ignoreForCollisions: true,
            dimensions: {
                x: 3,
                y: 3,
                z: 3
            },
            position: position,
            linePoints: [{
                x: 0,
                y: 0,
                z: 0
            }, extension],
            lineWidth: 5,
        };

        this.axis = Entities.addEntity(properties);
    },
    createSliderIndicator: function() {
        var position = Vec3.sum(this.basePosition, this.verticalOffset);
        //line starts on left and goes to right
        //set the end of the line to the right
        var rightVector = Quat.getRight(this.avatarRot);
        var initialDistance;
        if (this.sliderType === 'color_red') {
            initialDistance = this.distanceRed;
        }
        if (this.sliderType === 'color_green') {
            initialDistance = this.distanceGreen;
        }
        if (this.sliderType === 'color_blue') {
            initialDistance = this.distanceBlue;
        }
        if (this.sliderType === 'intensity') {
            initialDistance = this.distanceIntensity;
        }
        if (this.sliderType === 'cutoff') {
            initialDistance = this.distanceCutoff;
        }
        if (this.sliderType === 'exponent') {
            initialDistance = this.distanceExponent;
        }
        var extension = Vec3.multiply(initialDistance, rightVector);
        var sliderPosition = Vec3.sum(position, extension);
        var properties = {
            type: 'Sphere',
            name: 'Hifi-Slider::' + this.sliderType,
            dimensions: SLIDER_DIMENSIONS,
            collisionsWillMove: true,
            color: this.color,
            position: sliderPosition,
            script: SLIDER_SCRIPT_URL,
            userData: JSON.stringify({
                lightModifierKey: {
                    lightID: this.lightID,
                    sliderType: this.sliderType,
                    axisStart: position,
                    axisEnd: this.endOfAxis,
                }
            })
        };

        this.sliderIndicator = Entities.addEntity(properties);
    },

    setValueFromMessage: function(message) {

        //message is not for our light
        if (message.lightID !== this.lightID) {
            print('not our light')
            return;
        }

        //message is not our type
        if (message.sliderType !== this.sliderType) {
            print('not our slider type')
            return
        }

        var lightProperties = Entities.getEntityProperties(this.lightID);

        if (this.sliderType === 'color_red') {
            Entities.editEntity(this.lightID, {
                color: {
                    red: message.sliderValue,
                    green: lightProperties.color.green,
                    blue: lightProperties.color.blue
                }
            });
        }

        if (this.sliderType === 'color_green') {
            Entities.editEntity(this.lightID, {
                color: {
                    red: lightProperties.color.red,
                    green: message.sliderValue,
                    blue: lightProperties.color.blue
                }
            });
        }

        if (this.sliderType === 'color_blue') {
            Entities.editEntity(this.lightID, {
                color: {
                    red: lightProperties.color.red,
                    green: lightProperties.color.green,
                    blue: message.sliderValue,
                }
            });
        }

        if (this.sliderType === 'intensity') {
            Entities.editEntity(this.lightID, {
                intensity: message.sliderValue
            });
        }

        if (this.sliderType === 'cutoff') {
            Entities.editEntity(this.lightID, {
                cutoff: message.sliderValue
            });
        }

        if (this.sliderType === 'exponent') {
            Entities.editEntity(this.lightID, {
                exponent: message.sliderValue
            });
        }
    },
    setInitialSliderPositions: function() {
        this.distanceRed = (this.initialProperties.color.red / COLOR_MAX) * AXIS_SCALE;
        this.distanceGreen = (this.initialProperties.color.green / COLOR_MAX) * AXIS_SCALE;
        this.distanceBlue = (this.initialProperties.color.blue / COLOR_MAX) * AXIS_SCALE;
        this.distanceIntensity = (this.initialProperties.intensity / INTENSITY_MAX) * AXIS_SCALE;
        this.distanceCutoff = (this.initialProperties.cutoff / CUTOFF_MAX) * AXIS_SCALE;
        this.distanceExponent = (this.initialProperties.exponent / EXPONENT_MAX) * AXIS_SCALE;
    }

};

var sliders = [];
var slidersRef = {
    'color_red': null,
    'color_green': null,
    'color_blue': null,
    intensity: null,
    cutoff: null,
    exponent: null
}
var light = null;

function makeSliders(light) {
    if (light.type === 'spotlight') {
        var USE_COLOR_SLIDER = true;
        var USE_INTENSITY_SLIDER = true;
        var USE_CUTOFF_SLIDER = true;
        var USE_EXPONENT_SLIDER = true;
    }
    if (light.type === 'pointlight') {
        var USE_COLOR_SLIDER = true;
        var USE_INTENSITY_SLIDER = true;
        var USE_CUTOFF_SLIDER = false;
        var USE_EXPONENT_SLIDER = false;
    }
    if (USE_COLOR_SLIDER === true) {
        slidersRef.color_red = new entitySlider(light, RED, 'color_red', 1);
        slidersRef.color_green = new entitySlider(light, GREEN, 'color_green', 2);
        slidersRef.color_blue = new entitySlider(light, BLUE, 'color_blue', 3);

        sliders.push(slidersRef.color_red);
        sliders.push(slidersRef.color_green);
        sliders.push(slidersRef.color_blue);

    }
    if (USE_INTENSITY_SLIDER === true) {
        slidersRef.intensity = new entitySlider(light, WHITE, 'intensity', 4);
        sliders.push(slidersRef.intensity);
    }
    if (USE_CUTOFF_SLIDER === true) {
        slidersRef.cutoff = new entitySlider(light, PURPLE, 'cutoff', 5);
        sliders.push(slidersRef.cutoff);
    }
    if (USE_EXPONENT_SLIDER === true) {
        slidersRef.exponent = new entitySlider(light, PURPLE, 'exponent', 6);
        sliders.push(slidersRef.exponent);
    }
    subscribeToSliderMessages();
};

function subScribeToNewLights() {
    Messages.subscribe('Hifi-Light-Mod-Receiver');
    Messages.messageReceived.connect(handleLightModMessages);
}

function subscribeToSliderMessages() {
    Messages.subscribe('Hifi-Slider-Value-Reciever');
    Messages.messageReceived.connect(handleValueMessages);
}

function handleLightModMessages(channel, message, sender) {
    if (channel !== 'Hifi-Light-Mod-Receiver') {
        return;
    }
    if (sender !== MyAvatar.sessionUUID) {
        return;
    }
    var parsedMessage = JSON.parse(message);

    makeSliders(parsedMessage.light);
    light = parsedMessage.light.id
}

function handleValueMessages(channel, message, sender) {

    if (channel !== 'Hifi-Slider-Value-Reciever') {
        return;
    }
    //easily protect from other people editing your values, but group editing might be fun so lets try that first.
    // if (sender !== MyAvatar.sessionUUID) {
    //     return;
    // }
    var parsedMessage = JSON.parse(message);

    slidersRef[parsedMessage.sliderType].setValueFromMessage(parsedMessage);
}

function cleanup() {
    var i;
    for (i = 0; i < sliders.length; i++) {
        Entities.deleteEntity(sliders[i].axis);
        Entities.deleteEntity(sliders[i].sliderIndicator);
    }

    Messages.messageReceived.disconnect(handleLightModMessages);
    Messages.messageReceived.disconnect(handleValueMessages);
    Entities.deletingEntity.disconnect(deleteEntity);

}

Script.scriptEnding.connect(cleanup);
subScribeToNewLights();

function deleteEntity(entityID) {
    if (entityID === light) {
        //  cleanup();
    }
}


Entities.deletingEntity.connect(deleteEntity);


//other light properties
// diffuseColor: { red: 255, green: 255, blue: 255 },
// ambientColor: { red: 255, green: 255, blue: 255 },
// specularColor: { red: 255, green: 255, blue: 255 },
// constantAttenuation: 1,
// linearAttenuation: 0,
// quadraticAttenuation: 0,
// exponent: 0,
// cutoff: 180, // in degrees