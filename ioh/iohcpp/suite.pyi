from typing import Iterator, List

import ioh.iohcpp.logger

class BBOB(RealBase):
    def __init__(self, problem_ids: List[int], instances: List[int], dimensions: List[int]) -> None: ...

class Integer(IntegerBase):
    def __init__(self, problem_ids: List[int], instances: List[int], dimensions: List[int]) -> None: ...

class IntegerBase:
    def __init__(self, *args, **kwargs) -> None: ...
    def attach_logger(self, arg0: ioh.iohcpp.logger.Logger) -> None: ...
    def detach_logger(self) -> None: ...
    def reset(self) -> None: ...
    def __iter__(self) -> Iterator: ...
    def __len__(self) -> int: ...
    @property
    def dimensions(self) -> List[int]: ...
    @property
    def instances(self) -> List[int]: ...
    @property
    def name(self) -> str: ...
    @property
    def problem_ids(self) -> List[int]: ...

class PBO(IntegerBase):
    def __init__(self, problem_ids: List[int], instances: List[int], dimensions: List[int]) -> None: ...

class Real(RealBase):
    def __init__(self, problem_ids: List[int], instances: List[int], dimensions: List[int]) -> None: ...

class RealBase:
    def __init__(self, *args, **kwargs) -> None: ...
    def attach_logger(self, arg0: ioh.iohcpp.logger.Logger) -> None: ...
    def detach_logger(self) -> None: ...
    def reset(self) -> None: ...
    def __iter__(self) -> Iterator: ...
    def __len__(self) -> int: ...
    @property
    def dimensions(self) -> List[int]: ...
    @property
    def instances(self) -> List[int]: ...
    @property
    def name(self) -> str: ...
    @property
    def problem_ids(self) -> List[int]: ...
