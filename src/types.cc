export module types;

export enum class TurnPhase {
    PreRoll,
    Rolling,
    PostRoll,
    DebtResolution,
    TuitionDecision
};

export enum class PieceType {
    Goose,     // G
    GRTBus,    // B
    Doughnut,  // D
    Professor, // P
    Student,   // S
    Money,     // $
    Laptop,    // L
    PinkTie    // T
};

export char toChar(PieceType p) {
    switch (p) {
        case PieceType::Goose:     return 'G';
        case PieceType::GRTBus:    return 'B';
        case PieceType::Doughnut:  return 'D';
        case PieceType::Professor: return 'P';
        case PieceType::Student:   return 'S';
        case PieceType::Money:     return '$';
        case PieceType::Laptop:    return 'L';
        case PieceType::PinkTie:   return 'T';
    }
    return '?';
}

export PieceType fromChar(char c) {
    switch (c) {
        case 'G': return PieceType::Goose;
        case 'B': return PieceType::GRTBus;
        case 'D': return PieceType::Doughnut;
        case 'P': return PieceType::Professor;
        case 'S': return PieceType::Student;
        case '$': return PieceType::Money;
        case 'L': return PieceType::Laptop;
        case 'T': return PieceType::PinkTie;
        default:  return PieceType::Goose;
    }
}

export bool isValidPieceChar(char c) {
    return c == 'G' || c == 'B' || c == 'D' || c == 'P' ||
           c == 'S' || c == '$' || c == 'L' || c == 'T';
}

export enum class SLCEvent {
    Back3,
    Back2,
    Back1,
    Forward1,
    Forward2,
    Forward3,
    GoToTims,
    AdvanceToOSAP
};

export struct DiceResult {
    int die1 = 0;
    int die2 = 0;
    int sum = 0;
    bool isDoubles = false;
};
