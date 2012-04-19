# -*- coding: utf-8 -*-
# Copyright (C) 2011 Rosen Diankov <rosen.diankov@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
from common_test_openrave import *

class TestDatabases(EnvironmentSetup):
    def test_ikmodulegeneration(self):
        env=self.env
        self.LoadEnv('robots/neuronics-katana.zae')
        robot=env.GetRobots()[0]
        manip=robot.GetActiveManipulator()
        manip.SetIkSolver(None)
        ikmodule = RaveCreateModule(env,'ikfast')
        env.Add(ikmodule)
        out=ikmodule.SendCommand('LoadIKFastSolver %s %d 1'%(robot.GetName(),IkParameterizationType.TranslationDirection5D))
        assert(out is not None)
        assert(manip.GetIkSolver() is not None)
        
#     def test_database_paths(self):
#         pass
