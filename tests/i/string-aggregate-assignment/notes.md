# Aggregate assignment with STRING fields

Human review: pending. Default class assignment copies STRING fields independently, including self-assignment, without invoking the enclosing class's copy constructor or destructor. The two declared boxes are destroyed only when Check returns.
