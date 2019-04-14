//
//  GymAC.js
//
//  Script Type: AC
//  Created by Wil Bown on 1/3/19
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

// file:///home/wilbown/GitHub/hifi/script-archive/acScripts/GymAC.js

function getRndInt(min, max) { return Math.floor(Math.random() * (max - min + 1)) + min; }
function getRndFloat(min, max) { return Math.random() * (max - min) + min; }

// Agent.isAvatar = true; // getAvatarsInRange detects this script's Avatar if this is true
var POS_ZERO = { x: 0, y: 1, z: 0 };
var DIST_MAX = 40;
var DIST_NEAR = 5;

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

// Setup EntityViewer
EntityViewer.setPosition(POS_ZERO);
// EntityViewer.setOrientation(Quat.fromPitchYawRollDegrees(0, -90, 0));
EntityViewer.setCenterRadius(1000);

// var timePassed = 0.0;
// var updateSpeed = 5.0; // seconds
// function update(deltaTime) {
//     timePassed += deltaTime;
//     if (timePassed > updateSpeed) {
//         timePassed = 0.0;
//         EntityViewer.queryOctree();
//         // print("GymAC.update NEW");
//         // print("GymAC.update path: " + Script.resolvePath("/")); // "file://"
//         // print("GymAC.update path: " + Script.resolvePath("")); // file:///home/wilbown/GitHub/hifi/script-archive/acScripts/GymAC.js


//         // var position = Avatar.position;
//         // print("GymAC.update Avatar.position: " + JSON.stringify(position)); //{"x":0,"y":0,"z":0}

//         // var props = Entities.getEntityProperties("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}"); // gymSphere id
//         // print("GymAC.update Entities.position: " + JSON.stringify(props));

//         // var entityIDs = Entities.findEntities(position, 1000);
//         // var entityIDs = Entities.findEntities(0, 1000);
//         // var entityIDs = Entities.findEntities([0, 0, 0], 10);
//         // var entityIDs = Entities.findEntitiesByType("Shape", position, 1000);

//         // var entityIDs = AvatarList.getAvatarsInRange([0, 0, 0], 10); //works
//         // // var entityIDs = AvatarList.getAvatarIdentifiers(); //works, gets all on server
//         // // print("GymAC.update Entities.num entities: " + entityIDs.length);
//         // for (var key in entityIDs) {
//         //     var type = entityIDs[key];
//         //     // var type = Entities.getEntityProperties(entityIDs[key]).position;
//         //     // var type = AvatarList.getAvatar(entityIDs[key]);
//         //     print("GymAC.update Entities.num entities.type: " + JSON.stringify(type));
//         // }

//         // var props = Entities.getEntityProperties("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}");
//         // if (Object.keys(props).length !== 0) {
//         //     // print("GymAC.update Entities.position: " + JSON.stringify(props));
//         //     // Entities.editEntity("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}", { color: { red: getRndInt(0,255), green: getRndInt(0,255), blue: getRndInt(0,255)} });
//         //     // Entities.editEntity("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}", { userData: "testing" });
//         //     // Entities.editEntity("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}", { userData: JSON.stringify({ observation: [getRndFloat(-1.0,1.0), getRndFloat(-1.0,1.0), getRndFloat(-1.0,1.0), -0.35], reward: 0.03, done: false, info: {error: null}}) });
//         //     Entities.editEntity("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}", { userData: JSON.stringify({ observation: [getRndFloat(-1.0,1.0), getRndFloat(-1.0,1.0), getRndFloat(-1.0,1.0), -0.35], reward: 0.03}) });
//         // }

//         var props = Entities.getEntityProperties("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}");
//         if (Object.keys(props).length !== 0) {
//             var rwd = 0.0;
//             var obs = [1.0, 1.0, 1.0, 1.0];
//             var obsi = 0;
//             var entityIDs = AvatarList.getAvatarsInRange(props.position, 10); //works
//             for (i = 0; i < entityIDs.length; i++) {
//                 var avtr = AvatarList.getAvatar(entityIDs[i]);
//                 if (Object.keys(avtr).length !== 0) {
//                     var dist = Vec3.distance(props.position, avtr.position);
//                     if (dist > 10) continue;
//                     if (dist < 0.5) rwd += 1.0;
//                     if (dist < 0.0) dist = 0.0;
//                     obs[obsi] = dist / 5.0 - 1.0; obsi++;
//                 }
//             }
//             Entities.editEntity("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}", { userData: JSON.stringify({ observation: obs, reward: rwd}) });
//         }

//     }
// }


var agent = 5558;

var environment = { // observation = object, info = dictionary
    observation: [],
    reward: 0.0,
    done: false,
    info: {error: null},
}

var entityID = "{f3273714-5e19-4ceb-8ff1-67981dcc3890}";

function update(deltaTime) {
    if (!Entities.serversExist() || !Entities.canRez()) return;
    EntityViewer.queryOctree();

    var entity = Entities.getEntityProperties(this.entityID);
    if (Object.keys(entity).length === 0) return; // entity does not exist yet

    var rwd = 0.0;
    var obs = [];
    var obsi = 0;

    // Add entity position
    var edist = Vec3.distance(entity.position, POS_ZERO);
    var erads = Vec3.getAngle(entity.position, POS_ZERO);
    var epose = getPose(edist, erads);
    epose.forEach(function(e){ obs[obsi++] = e; });

    // Out of bounds, reset
    if (edist > DIST_MAX/2) {
        // print("GymAC.update: OUT OF BOUNDS " + edist);
        rwd -= 6.0;
        this.environment.done = true;
        Entities.editEntity(this.entityID, { color: COLOR_ZERO, position: POS_ZERO });
    }

    // See local avatars
    var avatarIDs = AvatarList.getAvatarsInRange(entity.position, DIST_MAX);
    for (i = 0; i < avatarIDs.length; i++) {
        var avtr = AvatarList.getAvatar(avatarIDs[i]);
        if (Object.keys(avtr).length === 0) continue; // avatar doesn't exist?

        var dist = Vec3.distance(entity.position, avtr.position);
        var rads = Vec3.getAngle(entity.position, avtr.position);

        if (dist > DIST_MAX) continue;
        if (dist < 0.1) {
            rwd -= 6.0;
            this.environment.done = true;
            Entities.editEntity(this.entityID, { color: COLOR_ZERO, position: POS_ZERO });
        }
        else if (dist < 0.5) rwd -= 1.0;
        else if (dist < 1.5) rwd += 0.2;
        else if (dist < 5) rwd += 0.03;
        else if (dist < 10) rwd += 0.01;

        var pose = getPose(dist, rads);
        pose.forEach(function(e){ obs[obsi++] = e; });
        if (obsi > 4*4*3) break; //max observation size
    }
    
    // Entities.editEntity(this.entityID, { userData:JSON.stringify({ reward:rwd, observation:obs }) });
    this.environment.reward = rwd;
    this.environment.observation = obs;
}

function getPose(dist, rads) {
    if (dist < 0.0) dist = 0.0;
    var near = (dist > DIST_NEAR) ? 255 : dist/DIST_NEAR*255;
    var far = dist/DIST_MAX*255;
    // print("GymAC.update rads: " + rads);
    var angle = rads/Math.PI*255;
    if (angle > 255) angle = 255;
    var pose = [];
    pose[0] = 255 - parseInt(near);
    pose[1] = 255 - parseInt(far);
    pose[2] = parseInt(angle);
    return pose;
}

// function handleGymAgentChange(_agent) {
//     print("GymAC.handleGymAgentChange _agent: " + _agent);
//     this.agent = _agent;

//     // TODO send gym action_space and observation_space vars to agent when first connecting

//     // action_space = spaces.Discrete(18)
//     // observation_space = spaces.Box(low=0, high=255, shape=(4,4,3), dtype=np.uint8)
//     // message = {
//     //     action_space: {low: [0.0, 0.0, 0.0], high: [1.0, 1.0, 1.0], type:"float32"},
//     //     observation_space: {low: [-1.0, -1.0, -1.0, -1.0], high: [1.0, 1.0, 1.0, 1.0], type:"float32"},
//     // };
//     // Gym.sendGymMessage(this.agent, message);
// }

function handleGymMessage(_message) {
    // print("GymAC.handleGymMessage agent: " + this.agent);
    // print("GymAC.handleGymMessage _message: "+JSON.stringify(_message));

    // if (_message.agent != this.agent) return; //ignore other agents

    // Execute action
    var action = _message.message.action[0];
    // print("GymAC.handleGymMessage: action: "+JSON.stringify(action));

    var entity = Entities.getEntityProperties(this.entityID);
    if (Object.keys(entity).length !== 0) { // entity does not exist
        if (action == "reset") {
            // Entities.editEntity(this.entityID, { color: COLOR_ZERO, position: POS_ZERO });
        } else if (action == 0) {
            Entities.editEntity(this.entityID, { position: Vec3.sum(entity.position, MOVE_X) });
        } else if (action == 1) {
            Entities.editEntity(this.entityID, { position: Vec3.sum(entity.position, MOVE_Y) });
        } else if (action == 2) {
            Entities.editEntity(this.entityID, { position: Vec3.sum(entity.position, MOVE_Z) });
        } else if (action == 3) {
            Entities.editEntity(this.entityID, { position: Vec3.subtract(entity.position, MOVE_X) });
        } else if (action == 4) {
            Entities.editEntity(this.entityID, { position: Vec3.subtract(entity.position, MOVE_Y) });
        } else if (action == 5) {
            Entities.editEntity(this.entityID, { position: Vec3.subtract(entity.position, MOVE_Z) });
        } else if (action == 6) {
            Entities.editEntity(this.entityID, { position: Vec3.sum(entity.position, MOVE_FAST_X) });
        } else if (action == 7) {
            Entities.editEntity(this.entityID, { position: Vec3.sum(entity.position, MOVE_FAST_Z) });
        } else if (action == 8) {
            Entities.editEntity(this.entityID, { position: Vec3.subtract(entity.position, MOVE_FAST_X) });
        } else if (action == 9) {
            Entities.editEntity(this.entityID, { position: Vec3.subtract(entity.position, MOVE_FAST_Z) });
        } else if (action == 15) {
            Entities.editEntity(this.entityID, { color: COLOR_MAD });
        } else if (action == 16) {
            Entities.editEntity(this.entityID, { color: COLOR_HAPPY });
        } else if (action == 17) {
            Entities.editEntity(this.entityID, { color: COLOR_COOL });
        } else {
            // Entities.editEntity(this.entityID, { color: { red: 255 * action[0], green: 255 * action[1], blue: 255 * action[2]} });
            // Entities.editEntity(this.entityID, { color: { red: 10 * action, green: 10 * action, blue: 10 * action} });
        }
    }

    // Return environment
    // this.update(0);
    Gym.sendGymMessage(this.agent, this.environment);
    if (this.environment.done) this.environment.done = false; // make sure to send, reset after send
}

function unload() {
    Script.update.disconnect(update);
    Gym.onGymMessage.disconnect(handleGymMessage);
    // Gym.onGymAgentChange.disconnect(handleGymAgentChange);
}

// Gym.onGymAgentChange.connect(handleGymAgentChange);
Gym.onGymMessage.connect(handleGymMessage);
Script.update.connect(update);
Script.scriptEnding.connect(unload);


// getAvatar = {"objectName":"",
// "position":{"x":-0.05963579937815666,"y":1.0846925973892212,"z":-0.2838750183582306},
// "scale":0.9283730387687683,
// "handPosition":{"x":-0.05963579937815666,"y":1.0846925973892212,"z":-0.2838750183582306},
// "bodyPitch":0.0018089902587234974,"bodyYaw":-60.243534088134766,"bodyRoll":0.0018089902587234974,
// "orientation":{"x":-0.000021576881408691406,"y":0.5018393397331238,"z":-0.000021576881408691406,"w":-0.8649608492851257},
// "headOrientation":{"x":-0.000021576881408691406,"y":0.5018393397331238,"z":-0.000021576881408691406,"w":-0.8649608492851257},
// "headPitch":0,"headYaw":0,"headRoll":0,
// "velocity":{"x":0,"y":0,"z":0},"angularVelocity":{"x":0,"y":0,"z":0},
// "sessionUUID":"{8f0ca9d4-feab-4f9c-b9c1-2cf5416dcc94}",
// "displayName":"Wil", "sessionDisplayName":"",
// "isReplicated":false,
// "lookAtSnappingEnabled":true,
// "skeletonModelURL":"https://s3-us-west-1.amazonaws.com/hifi-wilbown/avatar-BeingOfLight-mod/being_of_light.fst",
// "attachmentData":[],"jointNames":[],
// "audioLoudness":0,"audioAverageLoudness":0,
// "sensorToWorldMatrix":{"r0c0":0.5108550190925598,"r1c0":0.000016129144569276832,"r2c0":0.8935766220092773,"r3c0":0,"r0c1":-0.00006071057578083128,"r1c1":1.029296875,"r2c1":0.000016129144569276832,"r3c1":0,"r0c2":-0.8935766220092773,"r1c2":-0.00006071057578083128,"r2c2":0.5108550190925598,"r3c2":0,"r0c3":-0.061158500611782074,"r1c3":-0.17988932132720947,"r2c3":-0.28300338983535767,"r3c3":1},
// "controllerLeftHandMatrix":{"r0c0":1,"r1c0":0.00004315469413995743,"r2c0":-0.0000431528314948082,"r3c0":0,"r0c1":-0.0000431528314948082,"r1c1":1,"r2c1":0.00004315469413995743,"r3c1":0,"r0c2":0.00004315469413995743,"r1c2":-0.0000431528314948082,"r2c2":1,"r3c2":0,"r0c3":0,"r1c3":0,"r2c3":0,"r3c3":1},
// "controllerRightHandMatrix":{"r0c0":1,"r1c0":0.00004315469413995743,"r2c0":-0.0000431528314948082,"r3c0":0,"r0c1":-0.0000431528314948082,"r1c1":1,"r2c1":0.00004315469413995743,"r3c1":0,"r0c2":0.00004315469413995743,"r1c2":-0.0000431528314948082,"r2c2":1,"r3c2":0,"r0c3":0,"r1c3":0,"r2c3":0,"r3c3":1}}

// getEntityProperties = {"id":"{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}",
// "type":"Sphere",
// "created":"2019-01-03T08:52:31Z",
// "age":67172.65625,"ageAsText":"18 hours 39 minutes 32 seconds","lastEdited":1546572696951929,"lastEditedBy":"{ec1d12a0-f832-4edd-a25a-5cd573d4da8e}",
// "position":{"x":1.417858600616455,"y":1.280480146408081,"z":-1.5713622570037842},
// "dimensions":{"x":0.5,"y":0.5,"z":0.5},
// "naturalDimensions":{"x":1,"y":1,"z":1},"naturalPosition":{"x":0,"y":0,"z":0},
// "rotation":{"x":-0.0000152587890625,"y":-0.0000152587890625,"z":-0.0000152587890625,"w":1},
// "velocity":{"x":0,"y":0,"z":0},"gravity":{"x":0,"y":0,"z":0},"acceleration":{"x":0,"y":0,"z":0},"damping":0.39346998929977417,"restitution":0.5,"friction":0.5,"density":1000,
// "lifetime":-1,
// "script":"","scriptTimestamp":0,
// "serverScripts":"file:///home/wilbown/GitHub/hifi/scripts/tutorials/entity_scripts/gymSphere.js",
// "registrationPoint":{"x":0.5,"y":0.5,"z":0.5},
// "angularVelocity":{"x":0,"y":0,"z":0},"angularDamping":0.39346998929977417,
// "visible":true,"canCastShadow":true,
// "collisionless":true,"ignoreForCollisions":true,"collisionMask":31,"collidesWith":"static,dynamic,kinematic,myAvatar,otherAvatar,","dynamic":false,"collisionsWillMove":false,
// "href":"","description":"","actionData":"",
// "locked":false,
// "userData":"",
// "alpha":1,
// "itemName":"",
// "itemDescription":"",
// "itemCategories":"",
// "itemArtist":"","itemLicense":"","limitedRun":4294967295,"marketplaceID":"","editionNumber":0,"entityInstanceNumber":0,"certificateID":"","staticCertificateVersion":0,
// "name":"Gym Sphere",
// "collisionSoundURL":"",
// "shape":"Sphere",
// "color":{"red":47,"green":241,"blue":189},
// "boundingBox":{"brn":{"x":1.1678433418273926,"y":1.0304648876190186,"z":-1.8213775157928467},"tfl":{"x":1.6678738594055176,"y":1.5304954051971436,"z":-1.3213469982147217},"center":{"x":1.417858600616455,"y":1.280480146408081,"z":-1.5713622570037842},"dimensions":{"x":0.500030517578125,"y":0.500030517578125,"z":0.500030517578125}},
// "originalTextures":"{\n}\n",
// "parentID":"{00000000-0000-0000-0000-000000000000}","parentJointIndex":65535,
// "queryAACube":{"x":0.9848458766937256,"y":0.8474674224853516,"z":-2.0043749809265137,"scale":0.8660253882408142},
// "localPosition":{"x":1.417858600616455,"y":1.280480146408081,"z":-1.5713622570037842},
// "localRotation":{"x":-0.0000152587890625,"y":-0.0000152587890625,"z":-0.0000152587890625,"w":1},
// "localVelocity":{"x":0,"y":0,"z":0},"localAngularVelocity":{"x":0,"y":0,"z":0},
// "localDimensions":{"x":0.5,"y":0.5,"z":0.5},
// "entityHostType":"domain",
// "owningAvatarID":"{00000000-0000-0000-0000-000000000000}",
// "cloneable":false,"cloneLifetime":300,"cloneLimit":0,"cloneDynamic":false,"cloneAvatarEntity":false,"cloneOriginID":"{00000000-0000-0000-0000-000000000000}",
// "grab":{"grabbable":false,"grabKinematic":true,"grabFollowsController":true,"triggerable":true,"equippable":false,"equippableLeftPosition":{"x":0,"y":0,"z":0},"equippableLeftRotation":{"x":-0.0000152587890625,"y":-0.0000152587890625,"z":-0.0000152587890625,"w":1},"equippableRightPosition":{"x":0,"y":0,"z":0},"equippableRightRotation":{"x":-0.0000152587890625,"y":-0.0000152587890625,"z":-0.0000152587890625,"w":1},"equippableIndicatorURL":"","equippableIndicatorScale":{"x":1,"y":1,"z":1},"equippableIndicatorOffset":{"x":0,"y":0,"z":0}},
// "renderInfo":{},
// "clientOnly":false,"avatarEntity":false,"localEntity":false}

