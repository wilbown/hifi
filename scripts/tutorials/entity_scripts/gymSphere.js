//  gymSphere.js
//
//  Script Type: Entity
//  Created by Wil Bown on 12/21/2018
//  
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

(function() {

    // var COLOR_TEAL = { red: 0, green: 255, blue: 255 };
    // var COLOR_YELLOW = { red: 255, green: 255, blue: 0 };

    var _this;
    function GymSphere() {
        _this = this;
    }

    GymSphere.prototype = {
        entityID: null,
        agent: 5558,

        // clicked: false,
        environment: { // observation = object, info = dictionary
            observation: [0.0, 0.0, 0.0, 0.0],
            reward: 0.0,
            done: false,
            info: {error: null},
        },
        

        // remote global events, not in class namespace
        
        update: function(_deltaTime) {
            // print("GymSphere.update entityID:" + _this.entityID);
            // print("GymSphere.update Entities.canAdjustLocks():" + Entities.canAdjustLocks()); //true
            // Entities.editEntity(_this.entityID, { locked: true });
            var position = Entities.getEntityProperties(_this.entityID).position;
            print("GymSphere.update Entities.position: " + JSON.stringify(position)); //{"x":1.417858600616455,"y":1.280480146408081,"z":-1.5713622570037842}


            Script.update.disconnect(_this.update);
        },
        handleGymAgentChange: function(_agent) {
            print("GymSphere.handleGymAgentChange entityID:" + _this.entityID);
            _this.agent = _agent;

            // TODO send gym action_space and observation_space vars to agent when first connecting
            message = {
                action_space: {low: [0.0, 0.0, 0.0], high: [1.0, 1.0, 1.0], type:"float32"},
                observation_space: {low: [-1.0, -1.0, -1.0, -1.0], high: [1.0, 1.0, 1.0, 1.0], type:"float32"},
            };
            Gym.sendGymMessage(_this.agent, message);
        },
        handleGymMessage: function(_message) {
            // print("GymSphere.handleGymMessage entityID:" + _this.entityID);
            // print("GymSphere.handleGymMessage: "+JSON.stringify(_message));

            if (_message.agent != _this.agent) return; //ignore other agents
            action = _message.message.action;

            // Execute action
            Entities.editEntity(_this.entityID, { color: { red: 255 * action[0], green: 255 * action[1], blue: 255 * action[2]} });

            var userData = Entities.getEntityProperties(_this.entityID).userData;
            userData = JSON.parse(userData);
            // print("GymSphere.handleGymMessage: "+userData["observation"][3]);
            // print("GymSphere.handleGymMessage: "+JSON.stringify(userData));
            // print("GymSphere.handleGymMessage: "+JSON.stringify(userData.observation));

            // Return my current environment
            // _this.environment.observation = [action[1], Math.random(), action[2], -1.0];
            // _this.environment.reward = action[0];
            _this.environment.observation = [userData["observation"][0], userData["observation"][1], userData["observation"][2], userData["observation"][3]];
            _this.environment.reward = userData["reward"];
            Gym.sendGymMessage(_this.agent, _this.environment);
        },


        // builtin class events

        // enterEntity: function(_entityID) {
        //     print("Enter entity");
        //     Entities.editEntity(_entityID, {
        //         color: { red: 255, green: 64, blue: 64 },
        //     });
        // },
        // leaveEntity: function(_entityID) {
        //     print("Leave entity");
        //     Entities.editEntity(_entityID, {
        //         color: { red: 128, green: 128, blue: 128 },
        //     });
        // },
        // mousePressOnEntity: function(_entityID, mouseEvent) { 
        //     print("GymSphere.mousePressOnEntity entityID:" + _entityID);
        //     if (_this.clicked) {
        //         Entities.editEntity(_this.entityID, { color: COLOR_TEAL });
        //         _this.clicked = false;
        //     } else {
        //         Entities.editEntity(_this.entityID, { color: COLOR_YELLOW });
        //         _this.clicked = true;
        //     }
        // },
        preload: function(_entityID) {
            print("GymSphere.preload entityID:" + _entityID);
            _this.entityID = _entityID;

            Gym.onGymAgentChange.connect(_this.handleGymAgentChange);
            Gym.onGymMessage.connect(_this.handleGymMessage);
            Script.update.connect(_this.update);
        },
        unload: function(_entityID) {
            print("GymSphere.unload entityID:" + _entityID);

            // Tell agent that we are dying
            _this.environment.done = true;
            Gym.sendGymMessage(_this.agent, _this.environment);

            // Script.update.disconnect(_this.update);
            Gym.onGymMessage.disconnect(_this.handleGymMessage);
            Gym.onGymAgentChange.disconnect(_this.handleGymAgentChange);
        },

    };

    return new GymSphere();
});