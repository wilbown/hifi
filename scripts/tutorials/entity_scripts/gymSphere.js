//  gymSphere.js
//
//  Script Type: Entity
//  Created by Wil Bown on 12/21/2018
//  
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

(function() {
    var _this;

    function GymSphere() {
        _this = this;
        this.clicked = false;
        return;
    }
    
    GymSphere.prototype = {
        preload: function(entityID) {
            this.entityID = entityID;
            Gym.gymMessage.connect(function(eventData) {
                print("GymSphere.gymMessage: "+JSON.stringify(eventData));
                Entities.editEntity(entityID, { color: { red: 2*eventData.action, green: 2*eventData.action, blue: 2*eventData.action} });
            });
            Gym.registerActor(entityID);
            // Gym.sendRawGymMessage(0, 23);
            print("GymSphere.preload ID:" + entityID);
        },
        unload: function(entityID) {
            print("GymSphere.unload");
        },

        mousePressOnEntity: function(entityID, mouseEvent) { 
            print("GymSphere.mouseMoveOnEntity");
            if (this.clicked) {
                Entities.editEntity(entityID, { color: { red: 0, green: 255, blue: 255} });
                this.clicked = false;
                // Gym.sendRawDword(0, 1);
            } else {
                Entities.editEntity(entityID, { color: { red: 255, green: 255, blue: 0} });
                this.clicked = true;
                // Gym.sendRawDword(0, 0);
            }
        }

    };

    // entity scripts should return a newly constructed object of our type
    return new GymSphere();
});