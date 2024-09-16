game
3d camera
descriptor sets

Entity:
    addComponent<C>
    removeComponent<C>
    get<C>
    tryGet<C>
    hasComponent<C>
    delete<C>
    getString

Component:
    static ID: U12
    static NAME: string
    static SIZE: USize
    static ALIGN: USize

    deal with virtual / non trivially constructable
    require moveability