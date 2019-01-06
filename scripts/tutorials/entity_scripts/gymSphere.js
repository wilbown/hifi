//  gymSphere.js
//
//  Script Type: Entity
//  Created by Wil Bown on 12/21/2018
//  
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

(function() {
    var POS_ZERO = { x: 0, y: 1, z: 0 };
    var MOVE_X = { x: 0.01, y: 0, z: 0 };
    var MOVE_Y = { x: 0, y: 0.001, z: 0 };
    var MOVE_Z = { x: 0, y: 0, z: 0.01 };
    var MOVE_FAST_X = { x: 0.1, y: 0, z: 0 };
    var MOVE_FAST_Z = { x: 0, y: 0, z: 0.1 };

    var COLOR_ZERO = { red: 255, green: 255, blue: 0 };
    // var COLOR_TEAL = { red: 0, green: 255, blue: 255 };
    // var COLOR_YELLOW = { red: 255, green: 255, blue: 0 };
    var COLOR_MAD = { red: 255, green: 0, blue: 0 };
    var COLOR_HAPPY = { red: 0, green: 255, blue: 0 };
    var COLOR_COOL = { red: 0, green: 0, blue: 255 };

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

            // action_space = spaces.Discrete(18)
            // observation_space = spaces.Box(low=0, high=255, shape=(4,4,3), dtype=np.uint8)
            // message = {
            //     action_space: {low: [0.0, 0.0, 0.0], high: [1.0, 1.0, 1.0], type:"float32"},
            //     observation_space: {low: [-1.0, -1.0, -1.0, -1.0], high: [1.0, 1.0, 1.0, 1.0], type:"float32"},
            // };
            // Gym.sendGymMessage(_this.agent, message);
        },
        handleGymMessage: function(_message) {
            // print("GymSphere.handleGymMessage entityID:" + _this.entityID);
            // print("GymSphere.handleGymMessage: "+JSON.stringify(_message));

            if (_message.agent != _this.agent) return; //ignore other agents
            var entity = Entities.getEntityProperties(_this.entityID);
            action = _message.message.action[0];
            // print("GymSphere.handleGymMessage: action: "+JSON.stringify(action));

            // Execute action
            if (action == "reset") {
                Entities.editEntity(_this.entityID, { color: COLOR_ZERO, position: POS_ZERO });
            } else if (action == 0) {
                Entities.editEntity(_this.entityID, { position: Vec3.sum(entity.position, MOVE_X) });
            } else if (action == 1) {
                Entities.editEntity(_this.entityID, { position: Vec3.sum(entity.position, MOVE_Y) });
            } else if (action == 2) {
                Entities.editEntity(_this.entityID, { position: Vec3.sum(entity.position, MOVE_Z) });
            } else if (action == 3) {
                Entities.editEntity(_this.entityID, { position: Vec3.subtract(entity.position, MOVE_X) });
            } else if (action == 4) {
                Entities.editEntity(_this.entityID, { position: Vec3.subtract(entity.position, MOVE_Y) });
            } else if (action == 5) {
                Entities.editEntity(_this.entityID, { position: Vec3.subtract(entity.position, MOVE_Z) });
            } else if (action == 6) {
                Entities.editEntity(_this.entityID, { position: Vec3.sum(entity.position, MOVE_FAST_X) });
            } else if (action == 7) {
                Entities.editEntity(_this.entityID, { position: Vec3.sum(entity.position, MOVE_FAST_Z) });
            } else if (action == 8) {
                Entities.editEntity(_this.entityID, { position: Vec3.subtract(entity.position, MOVE_FAST_X) });
            } else if (action == 9) {
                Entities.editEntity(_this.entityID, { position: Vec3.subtract(entity.position, MOVE_FAST_Z) });
            } else if (action == 15) {
                Entities.editEntity(_this.entityID, { color: COLOR_MAD });
            } else if (action == 16) {
                Entities.editEntity(_this.entityID, { color: COLOR_HAPPY });
            } else if (action == 17) {
                Entities.editEntity(_this.entityID, { color: COLOR_COOL });
            } else {
                // Entities.editEntity(_this.entityID, { color: { red: 255 * action[0], green: 255 * action[1], blue: 255 * action[2]} });
                // Entities.editEntity(_this.entityID, { color: { red: 10 * action, green: 10 * action, blue: 10 * action} });
            }

            var sup_reward = 0.0;
            // Out of bounds, reset
            if (Vec3.distance(entity.position, POS_ZERO) > 20) {
                Entities.editEntity(_this.entityID, { color: COLOR_ZERO, position: POS_ZERO });
                print("GymSphere.handleGymMessage: OUT OF BOUNDS " + Vec3.distance(entity.position, POS_ZERO));
                sup_reward = -6.0;
                // _this.environment.done = true;
            }

            var userData = JSON.parse(entity.userData);
            // print("GymSphere.handleGymMessage: "+userData["observation"][3]);
            // print("GymSphere.handleGymMessage: "+JSON.stringify(userData));
            // print("GymSphere.handleGymMessage: "+JSON.stringify(userData.observation));

            // Return my current environment
            // _this.environment.reward = action[0];
            // _this.environment.observation = [action[1], Math.random(), action[2], -1.0];
            _this.environment.reward = userData["reward"] + sup_reward;
            // _this.environment.observation = [userData["observation"][0][0], userData["observation"][0][1], userData["observation"][0][2], userData["observation"][0][2]];
            _this.environment.observation = userData["observation"];
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

            // Tell agent that we are leaving
            _this.environment.done = true;
            Gym.sendGymMessage(_this.agent, _this.environment);

            // Script.update.disconnect(_this.update);
            Gym.onGymMessage.disconnect(_this.handleGymMessage);
            Gym.onGymAgentChange.disconnect(_this.handleGymAgentChange);
        },

    };

    return new GymSphere();
});