import functools
import json
from os import path

import eventlet
import eventlet.wsgi
import gymnasium
import socketio
from gymnasium.spaces import Box
from pettingzoo import ParallelEnv
from stable_baselines3 import PPO
from supersuit import pettingzoo_env_to_vec_env_v1
from supersuit.vector.sb3_vector_wrapper import SB3VecEnvWrapper

class Singleton:
    def __init__(self, decorated):
        self._decorated = decorated

    def instance(self):
        try:
            return self._instance
        except AttributeError:
            self._instance = self._decorated()
            return self._instance

    def __call__(self):
        raise TypeError('Singletons must be accessed through `instance()`.')

    def __instancecheck__(self, inst):
        return isinstance(inst, self._decorated)

@Singleton
class FyrecrackerNetworker:
    '''
    Handles the network layer between the game running in UE5 and
    this server.
    '''
    def __init__(self):
        self.sio = socketio.Server()
        self.timestep = 0
        self.last_timestep = 0
        
    def register_callbacks(
            self, 
            connection_callback, 
            disconnection_callback, 
            train_callback, 
            env_update_callback,
        ):
        '''
        Delegates business logic elsewhere.

        @param connection_callback What happens when a game connects
        @param disconnection_callback What happens when a game disconnects
        @param train_callback What happens when a game requests to train
        @param env_update_callback What happens when the game sends an environment update
        '''

        # Wrap the callback in a function that first parses the json
        # so the delegate doesn't have to
        def on_env_update(callback):
            def _on_env_update(sid, raw_json):
                json_struct = json.loads(raw_json)
                self.timestep += 1
                callback(
                    json_struct['observations'], 
                    json_struct['rewards'], 
                    json_struct['terminations'], 
                    json_struct['truncations'], 
                    json_struct['infos'],
                )
            return _on_env_update

        def on_start_train(callback):
            def _on_start_train(sid, raw_json):
                json_struct = json.loads(raw_json)
                callback(
                    json_struct['model_path'],
                )
            return _on_start_train
        
        # The other callbacks are naked
        self.sio.on('connect', connection_callback)
        self.sio.on('disconnect', disconnection_callback)
        self.sio.on('train', on_start_train(train_callback))
        self.sio.on('env_update', on_env_update(env_update_callback))

    def _block_until_update(self):
        '''
        Block until the game sends an update
        '''
        while True:
            if self.timestep > self.last_timestep:
                self.last_timestep = self.timestep
                break
            else:
                # Without this, I believe this while loop would steal
                # the cpu from all other event handlers
                self.sio.sleep(0.05)

    def emit_actions_and_block(self, actions):
        '''
        Send actions to the game and wait for the game to reply
        '''
        self.sio.emit('actions', json.dumps({'actions': actions}))
        self._block_until_update()
    
    def listen(self):
        '''
        Start the server. This call will not exit until the server
        closes.
        '''
        eventlet.wsgi.server(
            eventlet.listen(('', 3000)), 
            socketio.WSGIApp(self.sio),
        )

# The number of AIs in the environment
N_PLAYERS = 8

# The number of lasers from the agent
N_LASERS = 1
LASER_HIT_ENUM = dict(
    LASER_HIT_PLAYER = 0,
    LASER_HIT_FLOOR = 1,
)

# 3 floats for each x,y,z velocity coordinate
VELOCITY_VEC = 3

# 3 floats for each yaw,pitch,roll (in some order) rotation euler coordinate
LOOKING_VEC = 3

# boolean for if the agent can start the firing process
COOLDOWN = 1

# The size of the observation vector
OBSERVATION_DIM = \
    N_LASERS * 2 + \
    VELOCITY_VEC + \
    LOOKING_VEC + \
    COOLDOWN

@Singleton
class FyrecrackerEnvironment(ParallelEnv):
    '''
    The Fyrecracker PettingZoo multi-agent environment. This environment is bare-bones and only
    exchanges information with FyrecrackerNetworker. This environment blocks during 
    the .step() call and waits for the engine to send updated observations, rewards,
    etc. so that the next .step() call has actions for an up-to-date environment.
    '''

    metadata = {"render_modes": ["human"], "name": "fyrecracker_v1"}

    def __init__(self, render_mode=None):
        self.possible_agents = ["player_" + str(r) for r in range(N_PLAYERS)]
        self.agent_name_mapping = dict(
            zip(self.possible_agents, list(range(len(self.possible_agents))))
        )
        self.render_mode = render_mode

    def post_env_update(self, observations, rewards, terminations, truncations, infos):
        '''
        Call this function to update the environment to what was sent by the game
        '''
        self.observations = observations
        self.rewards = rewards
        self.terminations = terminations
        self.truncations = truncations
        self.infos = infos

    @functools.lru_cache(maxsize=None)
    def observation_space(self, agent):
        return Box(low=0.0, high=1.0, shape=(1, ))

    @functools.lru_cache(maxsize=None)
    def action_space(self, agent):
        return Box(low=0.0, high=1.0, shape=(1, ))

    def render(self):
        if self.render_mode is None:
            gymnasium.logger.warn(
                "You are calling render method without specifying any render mode."
            )
            return

        if len(self.agents) == 2:
            string = "TODO: This is a useless render message"
        else:
            string = "Game over"
        print(string)

    def close(self):
        """
        Close should release any graphical displays, subprocesses, network connections
        or any other environment data which should not be kept around after the
        user is no longer using the environment.
        """
        pass

    def reset(self, seed=None, options=None):
        self.agents = self.possible_agents[:]
        observations = {agent: [0.0] for agent in self.agents}
        infos = {agent: {} for agent in self.agents}

        return observations, infos

    def step(self, actions):

        # alert game of new actions and wait for reply
        FyrecrackerNetworker.instance().emit_actions_and_block(
            { key: value.tolist() for key, value in actions.items() }
        )

        #TODO should this be here?
        if self.render_mode == "human":
            self.render()

        # return new state (this is updated before emit_actions_and_block completes)
        return (
            self.observations, 
            self.rewards, 
            self.terminations, 
            self.truncations, 
            self.infos
        )

# keep ref to original environment
ue5_env = FyrecrackerEnvironment.instance()

# convert the pettingzoo environment into a vector gymnasium environment so that
# stable baselines 3 can use it. cannot use 'concat_vec_envs_v1' because cloudpickle 
# messes with the Networker singleton
env = SB3VecEnvWrapper(pettingzoo_env_to_vec_env_v1(ue5_env))

def connect_to_environment(*_):
    print('Fyrecracker Game has connected to server')

def disconnect_from_environment(*_):
    print('Fyrecracker Game has disconnected from server')

def start_train(model_path):

    # load / init
    if path.exists(model_path):
        model = PPO.load(model_path, env)
    else:
        model = PPO("MlpPolicy", env, verbose=1)
    
    # train
    model.learn(
        total_timesteps=250, 
        progress_bar=True,
    )
    
    # save
    model.save(model_path)

FyrecrackerNetworker.instance().register_callbacks(
    connection_callback=connect_to_environment,
    disconnection_callback=disconnect_from_environment,
    train_callback=start_train,
    env_update_callback=ue5_env.post_env_update,
)
FyrecrackerNetworker.instance().listen()







# # verify api
# from pettingzoo.test import parallel_api_test

# env = parallel_env(render_mode='human')
# parallel_api_test(env, num_cycles=1000)