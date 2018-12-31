//  gymSphere.js
//
//  Script Type: Entity
//  Created by Wil Bown on 12/21/2018
//  
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

(function() {

    var COLOR_TEAL = { red: 0, green: 255, blue: 255 };
    var COLOR_YELLOW = { red: 255, green: 255, blue: 0 };

    // var _this;
    function GymSphere() {
        // _this = this;
    }

    GymSphere.prototype = {
        entityID: null,
        clicked: false,

        _observation: 0,
        _reward: 0.0,
        _done: false,
        _info: 0,

        // class events
        update: function() {
            print("GymSphere.update entityID:" + this.entityID);
        },
        handleGymMessage: function(message) {
            print("GymSphere.handleGymMessage entityID:" + entityID);
            // print("GymSphere.handleGymMessage: "+JSON.stringify(message));
            Entities.editEntity(entityID, { color: { red: 2*message.action, green: 2*message.action, blue: 2*message.action} });
            Gym.sendRawGymMessage(message.agent, message.action);
        },

        // builtin events
        // mousePressOnEntity: function(entityID, mouseEvent) { 
        //     print("GymSphere.mousePressOnEntity entityID:" + entityID);
        //     if (this.clicked) {
        //         Entities.editEntity(entityID, { color: COLOR_TEAL });
        //         this.clicked = false;
        //     } else {
        //         Entities.editEntity(entityID, { color: COLOR_YELLOW });
        //         this.clicked = true;
        //     }
        // },
        preload: function(entityIDx) {
            print("GymSphere.preload entityID:" + entityIDx);
            entityID = entityIDx;

            // Script.update.connect(this.update);

            Gym.onGymMessage.connect(this.handleGymMessage);
            Gym.registerActor(entityID);

        },
        unload: function(entityIDx) {
            print("GymSphere.unload entityID:" + entityIDx);
            
            Gym.onGymMessage.disconnect(this.handleGymMessage);

            // Script.update.disconnect(this.update);

        },

    };

    return new GymSphere();
});